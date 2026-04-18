#pragma once

#include <algorithm>
#include <compare>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <immer/algorithm.hpp>
#include <immer/vector.hpp>
#include <immer/vector_transient.hpp>

#include <mine/mine-workspace.hxx>
#include <mine/mine-syntax.hxx>
#include <mine/mine-types.hxx>

#include <mine/mine-contract.hxx>

// Platform specifics.
//
#if defined(__unix__) || defined(__APPLE__)
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <unistd.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

namespace mine
{
  // Visual attributes for a single cell.
  //
  // We keep this structure relatively compact because we are going to store
  // thousands of these. Note that while modern terminals support 24-bit
  // TrueColor, we currently stick to the standard 8-bit palette (256 colors)
  // to minimize memory bandwidth usage during the diff pass.
  //
  struct cell_attributes
  {
    std::uint8_t fg = 7; // ANSI 37 (White)
    std::uint8_t bg = 0; // ANSI 40 (Black)

    bool bold      = false;
    bool italic    = false;
    bool underline = false;

    bool operator== (const cell_attributes&) const = default;
  };

  // The atomic unit of the screen grid.
  //
  // A "cell" here isn't just a `char`. It holds a complete "Grapheme Cluster"
  // because in the modern world, a single user-perceived character can be a
  // sequence of multiple bytes (UTF-8) or even multiple codepoints (like an
  // Emoji with a skin-tone modifier).
  //
  // Wide characters (CJK, Emoji) present a layout challenge. They visually
  // occupy two columns but strictly speaking belong to a single logical
  // position. We handle this by storing the data in the left cell and marking
  // the right cell as a "continuation". The renderer knows to skip these
  // continuations.
  //
  struct terminal_cell
  {
    std::string     text {" "};
    cell_attributes attrs;
    bool            wide_continuation {false};

    bool operator== (const terminal_cell&) const = default;
  };

  // The in-memory frame buffer.
  //
  // We use this for Double Buffering. The renderer maintains two instances:
  // `current` (what the user sees right now) and `next` (what we want to show
  // them in the next frame).
  //
  // Implementation-wise, we flatten the 2D grid into a single 1D vector. While
  // `vector<vector<cell>>` might seem more natural for a grid, it kills cache
  // locality.
  //
  class terminal_screen
  {
  public:
    using cells_type = immer::vector<terminal_cell>;

    terminal_screen () = default;

    explicit
    terminal_screen (screen_size s)
      : size_ (s),
        cells_ (s.rows * s.cols, terminal_cell {})
    {
    }

    terminal_screen (screen_size s, cells_type c)
      : size_ (s),
        cells_ (std::move (c))
    {
    }

    screen_size
    size () const noexcept
    {
      return size_;
    }

    // Access
    //

    const terminal_cell&
    at (screen_position p) const
    {
      MINE_PRECONDITION (size_.contains (p));
      return cells_[p.row * size_.cols + p.col];
    }

    const cells_type&
    cells () const noexcept
    {
      return cells_;
    }

    // Bulk Ops
    //

    void
    clear ()
    {
      cells_ = cells_type(size_.rows * size_.cols, terminal_cell {});
    }

    // Resize the canvas, preserving content.
    //
    // When the terminal window is resized, we want to try and keep the
    // current content "anchored" at top-left, rather than clearing everything.
    //
    // We compute the intersection of the old rect and the new rect. Content
    // inside the intersection is copied over; content outside is dropped; new
    // space is zero-initialized.
    //
    terminal_screen
    resize (screen_size new_s) const
    {
      auto t = immer::vector<terminal_cell>(new_s.rows * new_s.cols, terminal_cell{}).transient();

      std::uint16_t h (std::min (size_.rows, new_s.rows));
      std::uint16_t w (std::min (size_.cols, new_s.cols));

      for (std::uint16_t y (0); y < h; ++y)
      {
        for (std::uint16_t x (0); x < w; ++x)
        {
          screen_position p (y, x);
          t.set (y * new_s.cols + x, at (p));
        }
      }

      return terminal_screen (new_s, t.persistent ());
    }

    bool operator== (const terminal_screen&) const = default;

  private:
    screen_size size_ {24, 80};
    cells_type  cells_;
  };

  // A transient builder for rapid frame construction.
  //
  // Immutable data structures are expensive if we recreate the whole tree for
  // every single cell we write to the screen during the render pass. This
  // uses `immer::vector_transient` to allow O(1) mutations until we are
  // ready to snapshot the final frame via `finish()`.
  //
  class terminal_screen_builder
  {
  public:
    terminal_screen_builder (screen_size s)
      : size_ (s),
        cells_ (immer::vector<terminal_cell>(s.rows * s.cols, terminal_cell{}).transient())
    {
    }

    terminal_screen_builder (const terminal_screen& s)
      : size_ (s.size ()),
        cells_ (s.cells ().transient ())
    {
    }

    void
    set_cell (screen_position p, const terminal_cell &c)
    {
      cells_.set (p.row * size_.cols + p.col, c);
    }

    // Convenience helper for writing simple ASCII.
    //
    void
    set_char (screen_position p, char c, cell_attributes a = {})
    {
      set_cell (p, terminal_cell {std::string (1, c), a, false});
    }

    // Write a full grapheme cluster.
    //
    // If the grapheme is "wide" (like a Kanji or a Smiley), it will clobber two
    // cells. We write the actual content into `p` and then write a dummy marker
    // into `p + 1`.
    //
    // Note that we check bounds for the continuation cell: if a wide char is
    // written to the very last column, we just clip the continuation (the
    // terminal will likely wrap or clip anyway).
    //
    void
    set_grapheme (screen_position p,
                  std::string_view s,
                  cell_attributes a = {},
                  bool wide = false)
    {
      set_cell (p, terminal_cell {std::string (s), a, false});

      if (wide && p.col + 1 < size_.cols)
      {
        screen_position next (p.row, p.col + 1);
        set_cell (next, terminal_cell {"", a, true});
      }
    }

    void
    clear_line (std::uint16_t row)
    {
      MINE_PRECONDITION (row < size_.rows);

      for (std::uint16_t c = 0; c < size_.cols; ++c)
      {
        set_cell (screen_position (row, c), terminal_cell {});
      }
    }

    terminal_screen
    finish ()
    {
      return terminal_screen (size_, cells_.persistent ());
    }

    screen_size
    size () const noexcept
    {
      return size_;
    }

  private:
    screen_size size_;
    immer::vector_transient<terminal_cell> cells_;
  };

  // Diffing
  //

  // A list of point-mutations required to transition one frame to another.
  //
  struct screen_diff
  {
    struct change
    {
      screen_position pos;
      terminal_cell   cell;
    };

    std::vector<change> changes;

    bool
    empty () const noexcept
    {
      return changes.empty ();
    }
  };

  // Compute the minimal set of updates.
  //
  // This is the core optimization of the renderer. Writing to a TTY is
  // surprisingly expensive (syscall overhead, kernel buffering, potentially
  // network latency if over SSH).
  //
  // By comparing the previous frame (`old_s`) to the next frame (`new_s`) in
  // memory, we can emit ANSI codes *only* for the cells that actually
  // changed.
  //
  // Note that this requires both screens to have the same dimensions. If the
  // user resized the window, the whole coordinate system shifted, so a diff
  // is meaningless (and we should force a full redraw instead).
  //
  screen_diff
  compute_screen_diff (const terminal_screen& old_s,
                       const terminal_screen& new_s,
                       std::uint16_t row_start = 0,
                       std::uint16_t row_count = 0);

  // RAII wrapper for setting the terminal to "raw" mode.
  //
  // In raw mode, we disable all the helpful processing the TTY driver usually
  // does for us (echoing, line buffering, signal handling) so that we can
  // take full control of the input stream and rendering.
  //
  // Because messing up the terminal state leaves the user with a broken
  // shell (cursor hidden, no echo), we rely strictly on the destructor
  // to restore sanity.
  //
  class terminal_raw_mode
  {
  public:
    terminal_raw_mode ()
    {
      enable ();
    }

    ~terminal_raw_mode ()
    {
      disable ();
    }

    // Move-only semantics.
    //
    // We represent a unique ownership of the terminal state configuration.
    // Copying doesn't make sense (we can't restore the same state twice).
    //
    terminal_raw_mode (const terminal_raw_mode&) = delete;
    terminal_raw_mode& operator= (const terminal_raw_mode&) = delete;

    terminal_raw_mode (terminal_raw_mode&& other) noexcept
        : enabled_ (other.enabled_)
    {
#if defined(__unix__) || defined(__APPLE__)
      original_termios_ = other.original_termios_;
#elif defined(_WIN32)
      original_mode_ = other.original_mode_;
      input_handle_ = other.input_handle_;
#endif
      other.enabled_ = false;
    }

    terminal_raw_mode& operator= (terminal_raw_mode&& other) noexcept
    {
      if (this != &other)
      {
        disable ();

        enabled_ = other.enabled_;
#if defined(__unix__) || defined(__APPLE__)
        original_termios_ = other.original_termios_;
#elif defined(_WIN32)
        original_mode_ = other.original_mode_;
        input_handle_ = other.input_handle_;
#endif
        other.enabled_ = false;
      }
      return *this;
    }

    bool
    is_enabled () const noexcept { return enabled_; }

  private:
    bool enabled_ = false;

#if defined(__unix__) || defined(__APPLE__)
    struct termios original_termios_;

    void
    enable ()
    {
      if (enabled_)
        return;

      // Don't try to set raw mode on a pipe or file redirect.
      //
      if (!isatty (STDIN_FILENO))
        return;

      if (tcgetattr (STDIN_FILENO, &original_termios_) == -1)
        return;

      struct termios raw (original_termios_);

      // Input flags:
      // - BRKINT: Turn off break condition causing SIGINT.
      // - ICRNL:  Don't translate carriage return to newline on input.
      // - INPCK:  Disable parity checking.
      // - ISTRIP: Don't strip the 8th bit of input (we want UTF-8).
      // - IXON:   Disable software flow control (Ctrl-S/Ctrl-Q).
      //
      raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

      // Output flags:
      // - OPOST: Disable implementation-defined output processing.
      //
      raw.c_oflag &= ~(OPOST);

      // Control flags:
      // - CS8: Set character size to 8 bits per byte.
      //
      raw.c_cflag |= (CS8);

      // Local flags:
      // - ECHO:   Don't print what the user types.
      // - ICANON: Disable canonical mode (read byte-by-byte, not line-by-line).
      // - IEXTEN: Disable extended processing (like Ctrl-V).
      // - ISIG:   Disable signals (Ctrl-C/Z) so we can handle them ourselves.
      //
      raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

      // Control characters.
      //
      // Note that we must set VMIN to 1 rather than 0.
      //
      // It might be tempting to use VMIN=0 since we are operating in
      // non-blocking mode. However, doing so violates POSIX asynchronous
      // stream semantics in a way that breaks Boost.Asio.
      //
      // If VMIN is 0 and there is no input available, the terminal driver
      // makes read() return 0. To Asio, a 0-byte read unambiguously means
      // EOF. This causes the reactor to throw an EOF error, and if we
      // catch and continue, we end up in a tight loop spinning the CPU.
      //
      raw.c_cc[VMIN] = 1;
      raw.c_cc[VTIME] = 0;

      if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &raw) != -1)
      {
        // Enable Mouse Tracking.
        //
        // - 1000: Basic button press/release reporting.
        // - 1006: SGR extended coordinates.
        //
        // We explicitly enable SGR (1006) because the standard mouse encoding
        // overflows if the terminal is wider than 223 columns (255 - 32). SGR
        // uses a text-based format that supports any size.
        //
        // We ignore the write result because if stdout is broken, we have
        // bigger problems, and we are about to set enabled_ = true anyway.
        //
        const char enable_mouse[] = "\x1b[?1002h\x1b[?1006h";
        if (write (STDOUT_FILENO, enable_mouse, sizeof(enable_mouse) - 1) != -1)
          enabled_ = true;
        else
          tcsetattr (STDIN_FILENO, TCSAFLUSH, &original_termios_);
      }
    }

    void
    disable ()
    {
      if (enabled_)
      {
        // Disable Mouse Tracking.
        //
        // We must switch this off before returning control to the shell,
        // otherwise the user won't be able to select text in their terminal
        // using the mouse.
        //
        // ?1002l: Disable cell motion reporting. ?1006l: Disable SGR.
        //
        const char disable_mouse[] = "\x1b[?1002l\x1b[?1006l";
        write (STDOUT_FILENO, disable_mouse, sizeof (disable_mouse) - 1);
        tcsetattr (STDIN_FILENO, TCSAFLUSH, &original_termios_);
        enabled_ = false;
      }
    }

#elif defined(_WIN32)
    DWORD original_mode_ = 0;
    HANDLE input_handle_ = INVALID_HANDLE_VALUE;

    void
    enable ()
    {
      if (enabled_)
        return;

      input_handle_ = GetStdHandle (STD_INPUT_HANDLE);
      if (input_handle_ == INVALID_HANDLE_VALUE)
        return;

      if (!GetConsoleMode (input_handle_, &original_mode_))
        return;

      DWORD mode (original_mode_);

      // Disable standard processing to act like a raw TTY.
      //
      mode &= ~(ENABLE_LINE_INPUT |
                ENABLE_ECHO_INPUT |
                ENABLE_PROCESSED_INPUT);

      // Enable VT sequences so our ANSI parser works on Windows 10+.
      //
      mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

      if (SetConsoleMode (input_handle_, mode))
        enabled_ = true;
    }

    void
    disable ()
    {
      if (enabled_)
      {
        SetConsoleMode (input_handle_, original_mode_);
        enabled_ = false;
      }
    }
#else
    // Fallback for unsupported platforms.
    //
    void enable () {}
    void disable () {}
#endif
  };

  // Get the current terminal dimensions.
  //
  inline std::optional<screen_size>
  get_terminal_size ()
  {
#if defined(__unix__) || defined(__APPLE__)
    struct winsize ws;

    // We use STDOUT_FILENO because sometimes STDIN is redirected but
    // we still want the size of the output window.
    //
    if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
      return std::nullopt;

    return screen_size (ws.ws_row, ws.ws_col);

#elif defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h (GetStdHandle (STD_OUTPUT_HANDLE));

    if (h == INVALID_HANDLE_VALUE)
      return std::nullopt;

    if (!GetConsoleScreenBufferInfo (h, &csbi))
      return std::nullopt;

    // Windows rectangles are inclusive (Right - Left + 1).
    //
    return screen_size (
      static_cast<std::uint16_t> (csbi.srWindow.Bottom - csbi.srWindow.Top + 1),
      static_cast<std::uint16_t> (csbi.srWindow.Right - csbi.srWindow.Left + 1));
#else
    // If we can't determine size, returning a safe default (80x24) is
    // usually handled by the caller, so we return nullopt here to indicate
    // "unknown".
    //
    return std::nullopt;
#endif
  }

  class terminal_renderer
  {
  public:
    terminal_renderer ();

    explicit
    terminal_renderer (screen_size s);

    // Main rendering entry point.
    //
    // This builds the new frame, computes the difference against the
    // current frame, emits the ANSI codes, and updates the internal state.
    //
    void
    render (const workspace& state);

    // Optimized render for cursor movement only.
    //
    // If we know that only the cursor has moved (and no scrolling or text
    // edits occurred), we can skip the expensive screen build/diff process
    // and just emit the cursor jump code.
    //
    void
    render_cursor_only (const workspace& state);

    // Hard reset.
    //
    // Clears the physical screen, discards the internal buffer, and forces
    // a full redraw. Useful on startup or if the terminal output gets
    // garbled by external noise (e.g., printf debugging).
    //
    void
    force_redraw (const workspace& state);

    // Handle terminal window resize.
    //
    // We resize the internal buffer to match the new dimensions. The next
    // call to render() will fill the new area.
    //
    void
    resize (screen_size new_size);

    // Accessors.
    //
    const terminal_screen&
    current_screen () const noexcept { return current_screen_; }

  private:
    terminal_screen current_screen_ {screen_size (24, 80)};
    std::optional<cursor_position> last_cursor_pos_;

    syntax_highlighter highlighter_;

    bool frame_parity_ {false};

    // Composition helpers.
    //
    terminal_screen
    build_screen (const workspace& state) const;

    void
    draw_buffer (terminal_screen_builder& screen, const workspace& state) const;

    void
    draw_cmdline (terminal_screen_builder& screen, const workspace& state) const;

    // ANSI Output helpers.
    //
    void
    apply_diff (const screen_diff& diff);

    void
    position_cursor (const workspace& state);

    void
    move_cursor (screen_position pos);

    void
    set_attributes (cell_attributes attrs);

    void
    clear_screen ();

    void
    write (const std::string& str);

    void
    hide_cursor ();

    void
    show_cursor ();

    void
    begin_sync_update ();

    void
    end_sync_update ();
  };

  int
  estimate_grapheme_width (std::string_view g);
}
