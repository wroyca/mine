#include <mine/mine-terminal-render.hxx>
#include <mine/mine-assert.hxx>
#include <mine/mine-unicode-grapheme-iterator.hxx>

#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

namespace mine
{
  void terminal_renderer::
  render (const editor_state& s)
  {
    // First, let's hide the cursor.
    //
    // If we leave it on while blasting ANSI codes, it might jump around the
    // screen or flicker, which looks terrible. We'll turn it back on at the
    // exact right spot when we are done.
    //
    hide_cursor ();

    // Architecture: Double Buffering.
    //
    // We don't write directly to stdout. Instead, we build the "perfect"
    // version of what the screen *should* look like in memory (`next`). Then we
    // compare it to what we *think* is currently on screen (`current_screen_`).
    //
    // The idea here is to compute a minimal "diff" (e.g., "only char at 10,10
    // changed").
    //
    terminal_screen next (build_screen (s));

    // Handle terminal resize events that might have happened since the last
    // frame. If sizes mismatch, our "current" knowledge is garbage, so we
    // nuke it.
    //
    if (next.size () != current_screen_.size ())
    {
      clear_screen ();
      cout.flush ();
      current_screen_ = terminal_screen (next.size ());
    }

    screen_diff diff (compute_screen_diff (current_screen_, next));

    if (!diff.empty ())
    {
      apply_diff (diff);
      current_screen_ = move (next);
    }

    // Now that the paint is dry, put the hardware cursor where the logical
    // cursor is.
    //
    position_cursor (s);
    last_cursor_pos_ = s.get_cursor ().position ();

    cout.flush ();
  }

  void terminal_renderer::
  render_cursor_only (const editor_state& s)
  {
    // Optimize for the common cursor movement case.
    //
    // If the user just hit an arrow key, re-rendering and diffing the entire
    // screen content is wasteful. We know the text hasn't changed, only the
    // hardware cursor position and the status line (which displays the
    // coordinates) need updating.
    //
    hide_cursor ();

    // Update the status line.
    //
    // We clone the current screen to apply the status line updates. While
    // copying the buffer might seem heavy, it allows us to reuse the
    // standard diffing logic for the status row and avoids custom "draw
    // immediately" logic.
    //
    terminal_screen next (current_screen_);
    draw_status_line (next, s);

    // Diff only the last row.
    //
    // We know the content rows (0 to N-2) are identical, so we restrict
    // the diff scan to the status line to save cycles.
    //
    auto sz (current_screen_.size ());

    if (sz.rows > 0)
    {
      uint16_t r (sz.rows - 1);
      screen_diff diff (compute_screen_diff (current_screen_, next, r, 1));

      if (!diff.empty ())
      {
        apply_diff (diff);
        current_screen_ = std::move (next);
      }
    }

    position_cursor (s);
    last_cursor_pos_ = s.get_cursor ().position ();

    cout.flush ();
  }

  void terminal_renderer::
  force_redraw (const editor_state& s)
  {
    // The "Panic Button".
    //
    // Use this when the screen looks corrupted (e.g., some background job
    // printed garbage to stdout, or a network glitch messed up escape codes).
    // We discard our knowledge of the current state and force a full paint.
    //
    clear_screen ();
    current_screen_.clear ();
    last_cursor_pos_.reset ();

    render (s);
  }

  void terminal_renderer::
  resize (screen_size s)
  {
    // We don't draw immediately on resize signals (SIGWINCH). We just update
    // our internal geometry so the next `render()` call builds the correct
    // size buffer.
    //
    current_screen_ = current_screen_.resize (s);
  }

  // Screen Construction
  //

  // Helper: Guess how wide a grapheme is on screen.
  //
  // This is one of the hardest problems in TUI programming. We think a
  // character is 1 column, but the terminal (xterm, iterm2, etc.) might
  // render it as 2 columns (emoji, CJK) or 0 columns (combining marks).
  //
  // If we guess wrong, our rendering desyncs from the terminal's.
  //
  static int
  estimate_grapheme_width (string_view g)
  {
    if (g.empty ())
      return 0;

    // Fast path: ASCII is always 1.
    //
    unsigned char first (static_cast<unsigned char> (g[0]));
    if (first < 0x80)
      return 1;

    // Heuristic:
    // 0xE0-0xEF: 3-byte sequences (often CJK -> Wide).
    // 0xF0-0xFF: 4-byte sequences (often Emoji -> Wide).
    //
    // TODO: This is a hack. We should use a proper `wcwidth` implementation
    // or look up the Unicode East Asian Width property.
    //
    if (first >= 0xE0)
      return 2;

    return 1;
  }

  terminal_screen terminal_renderer::
  build_screen (const editor_state& s) const
  {
    terminal_screen scr (current_screen_.size ());

    draw_buffer (scr, s);
    draw_status_line (scr, s);

    return scr;
  }

  void terminal_renderer::
  draw_buffer (terminal_screen& scr, const editor_state& s) const
  {
    const auto& buf (s.buffer ());
    const auto& v (s.view ());
    auto sz (scr.size ());

    // Leave room for status line.
    //
    uint16_t rows (sz.rows > 0 ? sz.rows - 1 : 0);

    for (uint16_t r (0); r < rows; ++r)
    {
      line_number ln (v.top ().value + r);

      // Past EOF? Draw the "empty void" tilde.
      //
      if (ln.value >= buf.line_count ())
      {
        scr.set_char (screen_position (r, 0),
                      '~',
                      cell_attributes {.fg = 12}); // Bright Blue
        continue;
      }

      const auto& l (buf.line_at (ln));
      auto txt (l.view ());

      if (l.count () == 0)
        continue;

      // Rendering Text.
      //
      // We iterate logically (graphemes), but we must place them physically
      // (columns).
      //
      uint16_t col (0);

      const auto& seg (l.idx.get_segmentation ());
      grapheme_iterator it (&seg, 0);
      grapheme_iterator end (&seg, seg.size ());

      for (; it != end && col < sz.cols; ++it)
      {
        auto g (it->text (txt));
        int w (estimate_grapheme_width (g));

        if (w <= 0) w = 1;

        // Clip if it doesn't fit on the line.
        //
        if (col + static_cast<uint16_t> (w) > sz.cols)
          break;

        scr.set_grapheme (screen_position (r, col),
                          g,
                          cell_attributes {},
                          w == 2);

        col += static_cast<uint16_t> (w);
      }
    }
  }

  void terminal_renderer::
  draw_status_line (terminal_screen& scr, const editor_state& s) const
  {
    auto sz (scr.size ());
    if (sz.rows == 0)
      return;

    uint16_t row (sz.rows - 1);

    // Build status string: " Line X, Col Y [Modified]"
    //
    string st;
    st.reserve (64);

    st += " Line ";
    st += to_string (s.get_cursor ().line ().value + 1);
    st += ", Col ";
    st += to_string (s.get_cursor ().column ().value + 1);

    if (s.modified ())
      st += " [Modified]";

    if (st.size () > sz.cols)
      st.resize (sz.cols);

    // Render inverted (Black on Grey).
    //
    cell_attributes attr {.fg = 0, .bg = 7};

    uint16_t c (0);
    for (char ch : st)
    {
      scr.set_char (screen_position (row, c), ch, attr);
      ++c;
    }

    // Fill rest of line with background color.
    //
    while (c < sz.cols)
    {
      scr.set_char (screen_position (row, c), ' ', attr);
      ++c;
    }
  }

  // Low-level Output (ANSI)
  //

  void terminal_renderer::
  apply_diff (const screen_diff& d)
  {
    cell_attributes cur_attr;
    bool attr_set (false);

    // The Diff Applicator.
    //
    // We iterate through the list of changed cells.
    //
    // Performance Note:
    //
    // Ideally, we would detect contiguous runs of changes and emit a single
    // string print rather than jumping the cursor for every single cell. But
    // for now, correctness first.
    //
    for (const auto& c : d.changes)
    {
      // Wide chars (like kanji) take 2 cells. The second cell is a "dummy"
      // continuation. We skip it because drawing the first one fills both.
      //
      if (c.cell.wide_continuation)
        continue;

      move_cursor (c.pos);

      if (!attr_set || cur_attr != c.cell.attrs)
      {
        set_attributes (c.cell.attrs);
        cur_attr = c.cell.attrs;
        attr_set = true;
      }

      write (c.cell.text);
    }

    // Always clean up styles so we don't mess up the user's prompt after exit.
    //
    write ("\x1b[0m");
  }

  void terminal_renderer::
  position_cursor (const editor_state& s)
  {
    const auto& v (s.view ());
    const auto& c (s.get_cursor ());
    const auto& buf (s.buffer ());

    // Convert Logical (Line, Grapheme) -> Physical (Screen Row, Screen Col).
    //
    auto row (v.screen_row (c.line ()));

    if (row)
    {
      // To find the physical column, we have to sum the display widths of
      // every grapheme before the cursor. This is O(N) on line length.
      //
      uint16_t screen_col (0);

      if (c.column ().value > 0)
      {
        const auto& l (buf.line_at (c.line ()));
        auto txt (l.view ());

        std::size_t idx (0);
        for (grapheme_iterator it (&l.idx.get_segmentation (), 0);
             it != grapheme_iterator () && idx < c.column ().value;
             ++it, ++idx)
        {
          auto g (it->text (txt));
          int w (estimate_grapheme_width (g));
          if (w <= 0) w = 1;

          screen_col += static_cast<uint16_t> (w);
        }
      }

      move_cursor (screen_position (*row, screen_col));
      show_cursor ();
    }
    else
    {
      // If the logical cursor is scrolled off-screen, just hide the hardware
      // cursor so it doesn't mislead the user.
      //
      hide_cursor ();
    }
  }

  void terminal_renderer::
  move_cursor (screen_position p)
  {
    // ANSI CUP: ESC [ <row> ; <col> H (1-based)
    //
    write ("\x1b[" +
           to_string (p.row + 1) + ';' +
           to_string (p.col + 1) + 'H');
  }

  void terminal_renderer::
  set_attributes (cell_attributes a)
  {
    ostringstream oss;
    oss << "\x1b[0"; // Reset

    if (a.bold)      oss << ";1";
    if (a.italic)    oss << ";3";
    if (a.underline) oss << ";4";

    // Standard vs Bright colors.
    //
    if (a.fg < 8)
      oss << ";3" << static_cast<int> (a.fg);
    else
      oss << ";9" << static_cast<int> (a.fg - 8);

    if (a.bg < 8)
      oss << ";4" << static_cast<int> (a.bg);
    else
      oss << ";10" << static_cast<int> (a.bg - 8);

    oss << 'm';
    write (oss.str ());
  }

  void terminal_renderer::
  clear_screen ()
  {
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
    write ("\x1b[?25l");
  }

  void terminal_renderer::
  show_cursor ()
  {
    write ("\x1b[?25h");
  }
}
