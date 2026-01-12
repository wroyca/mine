#pragma once

#include <optional>
#include <utility> // move

#include <mine/mine-traits.hxx>
#include <mine/mine-assert.hxx>

// Platform specifics.
//
#if defined(__unix__) || defined(__APPLE__)
#  include <unistd.h>
#  include <termios.h>
#  include <sys/ioctl.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

namespace mine
{
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

      // Control characters:
      // VMIN=0, VTIME=0 makes read() non-blocking (return immediately).
      //
      // Note: Typically for TUI apps, VMIN=0/VTIME=1 (timeout) is safer,
      // but we assume the caller is using an async loop (select/epoll)
      // to wait for data before reading.
      //
      raw.c_cc[VMIN] = 0;
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
        if (write (STDOUT_FILENO, "\x1b[?1000h\x1b[?1006h", 20) != -1)
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
        // ?1000l: Disable button reporting. ?1006l: Disable SGR.
        //
        write (STDOUT_FILENO, "\x1b[?1000l\x1b[?1006l", 20);

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
}
