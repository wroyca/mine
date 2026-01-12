#include <mine/mine-terminal-render.hxx>
#include <mine/mine-assert.hxx>

#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

namespace mine
{
  // High-level rendering logic.
  //

  void terminal_renderer::
  render (const editor_state& s)
  {
    // Hide cursor during rendering to prevent flickering.
    //
    hide_cursor ();

    // We employ a double-buffering strategy here. We build the "ideal" next
    // frame in memory, compute the minimal set of changes required to transform
    // the current terminal state into that ideal state, and then issue only
    // those updates.
    //
    terminal_screen next (build_screen (s));

    // If the window resized between frames, our current screen state is
    // invalid. Just reset it and draw everything.
    //
    if (next.size () != current_screen_.size ())
    {
      clear_screen ();
      current_screen_ = terminal_screen (next.size ());
    }

    screen_diff diff (compute_screen_diff (current_screen_, next));

    if (!diff.empty ())
    {
      apply_diff (diff);
      current_screen_ = move (next);
    }

    // Always make sure the hardware cursor matches the logical cursor at the
    // end of the frame. position_cursor will show/hide the cursor based on
    // whether it's in view.
    //
    position_cursor (s);
    last_cursor_pos_ = s.get_cursor ().position ();

    cout.flush ();
  }

  void terminal_renderer::
  render_cursor_only (const editor_state& s)
  {
    // Optimization for simple navigation. If we know only the cursor moved
    // and no content changed, we skip the expensive screen build/diff cycle.
    // position_cursor will show/hide the cursor based on whether it's in view.
    //
    hide_cursor ();
    position_cursor (s);
    last_cursor_pos_ = s.get_cursor ().position ();

    cout.flush ();
  }

  void terminal_renderer::
  force_redraw (const editor_state& s)
  {
    // Sometimes the terminal state gets desynchronized (e.g., external program
    // output, network glitch). This is the "nuke it from orbit" option.
    //
    clear_screen ();
    current_screen_.clear ();
    last_cursor_pos_.reset ();

    render (s);
  }

  void terminal_renderer::
  resize (screen_size s)
  {
    // We don't actually draw here; we just resize our internal buffer so the
    // next render() call knows what canvas size to work with.
    //
    current_screen_ = current_screen_.resize (s);
  }

  // Screen construction.
  //

  terminal_screen terminal_renderer::
  build_screen (const editor_state& s) const
  {
    terminal_screen scr (current_screen_.size ());

    // Draw the actual file content.
    //
    draw_buffer (scr, s);

    // Overlay the UI elements (status line, etc).
    //
    draw_status_line (scr, s);

    return scr;
  }

  void terminal_renderer::
  draw_buffer (terminal_screen& scr, const editor_state& s) const
  {
    const text_buffer& buf (s.buffer ());
    const class view& view (s.view ());
    screen_size size (scr.size ());

    // Reserve the bottom row for the status line.
    //
    uint16_t rows (size.rows > 0 ? size.rows - 1 : 0);

    for (uint16_t r (0); r < rows; ++r)
    {
      line_number ln (view.top ().value + r);

      // If we are past the end of the file, draw the classic vim-style tilde.
      //
      if (ln.value >= buf.line_count ())
      {
        scr.set_char (screen_position (r, 0),
                      '~',
                      cell_attributes {.fg_color = 12}); // Bright Blue.
        continue;
      }

      const auto& line (buf.line_at (ln));

      // Simple rendering: just copy chars until we hit the screen edge.
      //
      // TODO: Handle multibyte characters and grapheme clusters correctly.
      // Currently, we assume 1 byte == 1 column, which is wrong for UTF-8.
      //
      uint16_t c (0);
      for (char ch : line)
      {
        if (c >= size.cols)
          break;

        scr.set_char (screen_position (r, c), ch);
        ++c;
      }
    }
  }

  void terminal_renderer::
  draw_status_line (terminal_screen& scr, const editor_state& s) const
  {
    screen_size size (scr.size ());
    if (size.rows == 0)
      return;

    uint16_t row (size.rows - 1);

    // Format: " Line X, Col Y [Modified]"
    //
    string st;
    st.reserve (64);

    st += " Line ";
    st += to_string (s.get_cursor ().line ().value + 1);
    st += ", Col ";
    st += to_string (s.get_cursor ().column ().value + 1);

    if (s.modified ())
      st += " [Modified]";

    // Truncate the status line to fit terminal width.
    //
    if (st.size () > size.cols)
      st.resize (size.cols);

    // Render inverted (black on white/grey).
    //
    cell_attributes attr {.fg_color = 0, .bg_color = 7};

    uint16_t c (0);
    for (char ch : st)
    {
      scr.set_char (screen_position (row, c), ch, attr);
      ++c;
    }

    // Pad the rest of the line with empty space to maintain the background
    // color.
    //
    while (c < size.cols)
    {
      scr.set_char (screen_position (row, c), ' ', attr);
      ++c;
    }
  }

  // Low-level output (ANSI sequences).
  //

  void terminal_renderer::
  apply_diff (const screen_diff& diff)
  {
    cell_attributes curr_attr;
    bool attr_set (false);

    // Iterate through the minimal set of changes.
    //
    // Note: This is currently suboptimal for long runs of text because we
    // move the cursor for *every* changed cell in the diff. A smarter diff
    // would group contiguous changes into strings.
    //
    for (const auto& change : diff.changes)
    {
      move_cursor (change.pos);

      if (!attr_set || curr_attr != change.cell.attrs)
      {
        set_attributes (change.cell.attrs);
        curr_attr = change.cell.attrs;
        attr_set = true;
      }

      // TODO: Buffer this?
      //
      cout.put (change.cell.ch);
    }

    // Always reset attributes at the end to avoid leaking style into the
    // shell if we crash.
    //
    write ("\x1b[0m");
  }

  void terminal_renderer::
  position_cursor (const editor_state& s)
  {
    const class view& view (s.view ());
    const cursor& cur (s.get_cursor ());

    // Map the logical buffer position to the physical screen row.
    //
    auto row (view.screen_row (cur.line ()));

    if (row)
    {
      screen_position pos (*row, cur.column ().value);
      move_cursor (pos);
      show_cursor ();
    }
    else
    {
      // Cursor is out of view, hide it.
      //
      hide_cursor ();
    }
  }

  void terminal_renderer::
  move_cursor (screen_position pos)
  {
    // ANSI is 1-based.
    // ESC [ <row> ; <col> H
    //
    write ("\x1b[" +
           to_string (pos.row + 1) + ';' +
           to_string (pos.col + 1) + 'H');
  }

  void terminal_renderer::
  set_attributes (cell_attributes attr)
  {
    ostringstream oss;
    oss << "\x1b[0"; // Start with reset.

    if (attr.bold)      oss << ";1";
    if (attr.italic)    oss << ";3";
    if (attr.underline) oss << ";4";

    // Colors:
    // 30-37: Standard FG
    // 90-97: Bright FG
    // 40-47: Standard BG
    // 100-107: Bright BG
    //
    if (attr.fg_color < 8)
      oss << ";3" << static_cast<int> (attr.fg_color);
    else
      oss << ";9" << static_cast<int> (attr.fg_color - 8);

    if (attr.bg_color < 8)
      oss << ";4" << static_cast<int> (attr.bg_color);
    else
      oss << ";10" << static_cast<int> (attr.bg_color - 8);

    oss << 'm';
    write (oss.str ());
  }

  void terminal_renderer::
  clear_screen ()
  {
    // \x1b[2J: Clear entire screen.
    // \x1b[H:  Move cursor to top-left (1,1).
    //
    write ("\x1b[2J\x1b[H");
  }

  void terminal_renderer::
  write (const string& s)
  {
    cout << s;
  }

  void terminal_renderer::
  hide_cursor ()
  {
    // ANSI escape code: ESC [ ? 25 l (DECTCEM - hide cursor)
    //
    write ("\x1b[?25l");
  }

  void terminal_renderer::
  show_cursor ()
  {
    // ANSI escape code: ESC [ ? 25 h (DECTCEM - show cursor)
    //
    write ("\x1b[?25h");
  }
}
