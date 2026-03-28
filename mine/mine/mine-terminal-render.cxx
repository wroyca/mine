#include <mine/mine-terminal-render.hxx>
#include <mine/mine-contract.hxx>
#include <mine/mine-unicode.hxx>

#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

namespace mine
{
  namespace
  {
    // Map our semantic tokens to standard ANSI indexed colors. We try to use
    // bright variants where it makes sense to maintain good contrast.
    //
    static uint8_t
    map_token_color (syntax_token_type t)
    {
      switch (t)
      {
        case syntax_token_type::keyword:  return 5; // Magenta
        case syntax_token_type::string:   return 2; // Green
        case syntax_token_type::type:     return 3; // Yellow
        case syntax_token_type::function: return 4; // Blue
        case syntax_token_type::variable: return 6; // Cyan
        case syntax_token_type::constant: return 1; // Red
        case syntax_token_type::comment:  return 8; // Bright Black (Grey)
        default:                          return 7; // White
      }
    }
  }

  terminal_renderer::
  terminal_renderer ()
  {
    highlighter_.init ();
  }

  terminal_renderer::
  terminal_renderer (screen_size s)
    : current_screen_ (s)
  {
    highlighter_.init ();
  }

  void terminal_renderer::
  render (const editor_state& s)
  {
    // Make sure the highlighter is up to date with the latest buffer contents
    // before we start projecting the view onto the screen.
    //
    highlighter_.update (s.buffer ());

    // Begin synchronized update (DEC Private Mode 2026).
    //
    // This tells the terminal to buffer all output until we call
    // end_sync_update.
    //
    begin_sync_update ();

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

    end_sync_update ();
    cout.flush ();
  }

  void terminal_renderer::
  render_cursor_only (const editor_state& s)
  {
    // Begin synchronized update.
    //
    begin_sync_update ();

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
    // copying the buffer might seem heavy, it allows us to reuse the standard
    // diffing logic for the status row and avoids custom "draw immediately"
    // logic.
    //
    terminal_screen_builder next_builder (current_screen_);

    // Since a layout split heavily multiplexes rendering surfaces per grid
    // square, any cursor activity usually mandates regenerating all context
    // rows to avoid orphaned dirty pixels in the terminal pipeline.
    //
    draw_buffer (next_builder, s);
    draw_cmdline (next_builder, s);

    terminal_screen next (next_builder.finish ());

    screen_diff diff (compute_screen_diff (current_screen_, next));

    if (!diff.empty ())
    {
      apply_diff (diff);
      current_screen_ = std::move (next);
    }

    position_cursor (s);
    last_cursor_pos_ = s.get_cursor ().position ();

    end_sync_update ();
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
  int
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
    terminal_screen_builder scr (current_screen_.size ());

    draw_buffer (scr, s);
    draw_cmdline (scr, s);

    return scr.finish ();
  }

  void terminal_renderer::
  draw_buffer (terminal_screen_builder& ts, const editor_state& s) const
  {
    auto sz (ts.size ());
    std::vector<window_layout> lays;

    // Reserve bottom-most row exclusively for the prompt stream handler.
    //
    s.get_layout (lays, sz.cols, sz.rows > 0 ? sz.rows - 1 : 0);

    for (const auto& lay : lays)
    {
      const auto& ws (s.get_window (lay.win));
      const auto& bs (s.get_buffer (ws.buf));
      bool is_act (s.active_window () == lay.win);

      const auto& b (bs.content);
      const auto& v (ws.vw);
      const auto& c (ws.cur);

      // Figure out the selection bounds upfront. We normalize them so that the
      // start is always before the end, which makes our hit-testing during
      // rendering trivial.
      //
      bool hs (c.has_mark ());
      cursor_position ss (c.position ());
      cursor_position se (c.position ());

      if (hs)
      {
        ss = min (c.mark (), c.position ());
        se = max (c.mark (), c.position ());
      }

      uint16_t rws (lay.h > 1 ? lay.h - 1 : 0);
      uint16_t lim (lay.w);

      // Prepare our syntax highlights for the viewport lines so we don't query
      // the whole tree.
      //
      size_t start_line (v.top ().value);
      size_t end_line (start_line + rws);
      auto highlights (highlighter_.query_lines (start_line, end_line));

      for (uint16_t r (0); r < rws; ++r)
      {
        line_number ln (v.top ().value + r);

        // Are we past the end of the file? If so, we just draw the standard empty
        // void tilde and move on to the next row.
        //
        if (ln.value >= b.line_count ())
        {
          cell_attributes ca;
          ca.fg = 12;

          ts.set_char (screen_position (lay.y + r, lay.x + 0), '~', ca);
          continue;
        }

        const auto& l (b.line_at (ln));

        // Fast path for empty lines. We only need to care about whether this line
        // falls inside an active multi-line selection.
        //
        if (l.count () == 0)
        {
          cursor_position ep (ln, column_number (0));

          // Note that we let the hardware cursor double as visual selection
          // highlight to prevent double-inversion.
          //
          if (hs && ep >= ss && ep <= se && ep != c.position () && lim > 0)
          {
            cell_attributes ca;
            ca.fg = 0;
            ca.bg = 7;
            ts.set_char (screen_position (lay.y + r, lay.x + 0), ' ', ca);
          }
          continue;
        }

        auto txt (l.view ());
        const auto& seg (l.idx.get_segmentation ());

        // Render the text.
        //
        // We iterate over the logical graphemes but we have to place them
        // physically into screen columns. This means we have to keep track of
        // both logical and physical progression.
        //
        auto rng (make_grapheme_range (seg));

        uint16_t col (0);
        std::size_t lc (0);

        // Note that we check 'col < lim' inside the loop rather than in the
        // condition. That is, we need to handle wide characters by preventing
        // partial drawing if a double-width char exceeds the edge.
        //
        for (auto i (rng.begin ()); i != rng.end (); ++i)
        {
          if (col >= lim) break;

          auto g (i->text (txt));
          int w (estimate_grapheme_width (g));

          // Sanity check for weird control characters or zero-width joiners that
          // might throw off our physical column count.
          //
          if (w <= 0) w = 1;

          // Clip if the grapheme doesn't fit on the remainder of the line.
          //
          if (col + static_cast<uint16_t> (w) > lim) break;

          cursor_position cp (ln, column_number (lc));

          // Apply our syntax highlighting based on the byte offset of the
          // current grapheme.
          //
          syntax_token_type token (syntax_token_type::none);
          size_t off (i->byte_offset);

          for (const auto& hl : highlights)
          {
            if (ln.value > hl.start_line ||
               (ln.value == hl.start_line && off >= hl.start_col_byte))
            {
              if (ln.value < hl.end_line ||
                 (ln.value == hl.end_line && off < hl.end_col_byte))
              {
                token = hl.type;
              }
            }
          }

          cell_attributes ca;
          ca.fg = map_token_color (token);

          // Exclude the cell under the cursor from explicit highlight coloring to
          // avoid "double inversion" where the hardware cursor sits.
          //
          bool sel (hs && cp >= ss && cp <= se && cp != c.position ());

          if (sel)
          {
            ca.fg = 0;
            ca.bg = 7;
          }

          ts.set_grapheme (screen_position (lay.y + r, lay.x + col), g, ca, w == 2);

          col += static_cast<uint16_t> (w);
          lc++;
        }

        cursor_position ep (ln, column_number (lc));

        // We also need to handle the multi-line selection wrap. If the selection
        // spans across lines, we draw an inverted space at the end of the line to
        // visually indicate the newline is selected.
        //
        if (hs && ep >= ss && ep <= se && ep != c.position () && col < lim)
        {
          cell_attributes ca;
          ca.fg = 0;
          ca.bg = 7;
          ts.set_char (screen_position (lay.y + r, lay.x + col), ' ', ca);
        }
      }

      if (lay.h > 1)
      {
        uint16_t row (lay.y + lay.h - 1);

        // Build status string: " Line X, Col Y [Modified]"
        //
        string st (" Line " + to_string (c.line ().value + 1) + ", Col " + to_string (c.column ().value + 1));

        if (bs.modified)
          st += "[Modified]";

        if (st.size () > lay.w)
          st.resize (lay.w);

        // Render inverted (Black on Grey for active, Black on Dark Grey for inactive).
        //
        cell_attributes attr {.fg = 0, .bg = static_cast<uint8_t> (is_act ? 7 : 8)};

        uint16_t cc (0);
        for (char ch : st)
        {
          ts.set_char (screen_position (row, lay.x + cc), ch, attr);
          ++cc;
        }

        // Fill rest of line with background color.
        //
        while (cc < lay.w)
        {
          ts.set_char (screen_position (row, lay.x + cc), ' ', attr);
          ++cc;
        }
      }
    }
  }

  void terminal_renderer::
  draw_cmdline (terminal_screen_builder& scr, const editor_state& s) const
  {
    auto sz (scr.size ());
    if (sz.rows == 0) return;

    uint16_t row (sz.rows - 1);

    string st;
    if (s.cmdline ().active)
      st = ":" + s.cmdline ().content;
    else
      st = s.cmdline ().message;

    if (st.size () > sz.cols)
      st.resize (sz.cols);

    cell_attributes attr {.fg = 7, .bg = 0};

    uint16_t c (0);
    for (char ch : st)
    {
      scr.set_char (screen_position (row, c), ch, attr);
      ++c;
    }

    while (c < sz.cols)
    {
      scr.set_char (screen_position (row, c), ' ', attr);
      ++c;
    }
  }

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
    auto& cl (s.cmdline ());

    if (cl.active)
    {
      auto sz (current_screen_.size ());

      // Determine the bottom row. Protect against underflow if the screen size
      // happens to be temporarily bogus (for example, 0 rows during a rapid
      // terminal resize).
      //
      uint16_t r (sz.rows > 0 ? sz.rows - 1 : 0);

      // Start at column 1 to account for the command prompt character (':').
      //
      uint16_t c (1);

      std::string_view v (cl.content);
      std::size_t p (cl.cursor_pos);
      std::size_t i (0);

      // Advance through the content up to the cursor position to calculate its
      // true visual width on the screen. We have to do this grapheme by
      // grapheme since UTF-8 sequences can have varying terminal widths.
      //
      while (i < p && i < v.size ())
      {
        std::size_t n (next_grapheme_boundary (v, i));
        int w (estimate_grapheme_width (v.substr (i, n - i)));

        // Treat zero or negative width graphemes as width 1 so we don't end up
        // overlapping characters visually if the estimator gets confused by
        // weird terminal states.
        //
        c += static_cast<uint16_t> (w > 0 ? w : 1);
        i = n;
      }

      move_cursor (screen_position (r, c));
      show_cursor ();
      return;
    }

    auto sz (current_screen_.size ());
    std::vector<window_layout> lays;
    s.get_layout (lays, sz.cols, sz.rows > 0 ? sz.rows - 1 : 0);

    const window_layout* aw (nullptr);

    for (const auto& l : lays)
    {
      if (l.win == s.active_window ())
      {
        aw = &l;
        break;
      }
    }

    if (!aw)
    {
      hide_cursor ();
      return;
    }

    const auto& ws (s.get_window (aw->win));
    const auto& bs (s.get_buffer (ws.buf));

    const auto& v (ws.vw);
    const auto& c (ws.cur);
    const auto& buf (bs.content);

    // Convert Logical (Line, Grapheme) -> Physical (Screen Row, Screen Col).
    //
    auto row (v.screen_row (c.line ()));

    if (row)
    {
      // To find the physical column, we have to sum the display widths of
      // every grapheme before the cursor. This is O(N) on line length.
      //
      uint16_t screen_col (0);

      // Fast path: if cursor is at column 0, width is 0.
      //
      if (c.column ().value > 0)
      {
        const auto& l (buf.line_at (c.line ()));
        auto txt (l.view ());
        const auto& seg (l.idx.get_segmentation ());
        auto rng (make_grapheme_range (seg));

        // We need to process 'target' number of graphemes.
        //
        std::size_t target (c.column ().value);

        for (auto it (rng.begin ()); it != rng.end () && target > 0; ++it, --target)
        {
          int w (estimate_grapheme_width (it->text (txt)));

          if (w <= 0) w = 1;

          screen_col += static_cast<uint16_t> (w);
        }
      }

      // Respect localized layout grid geometry offsets so hardware blinking
      // correlates logically to individual child surfaces without visual shearing.
      //
      move_cursor (screen_position (aw->y + *row, aw->x + screen_col));
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
    // ANSI CUP: ESC[ <row> ; <col> H (1-based)
    //
    write ("\x1b[" + to_string (p.row + 1) + ';' + to_string (p.col + 1) + 'H');
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
    if (a.fg < 8) oss << ";3" << static_cast<int> (a.fg);
    else          oss << ";9" << static_cast<int> (a.fg - 8);

    if (a.bg < 8) oss << ";4" << static_cast<int> (a.bg);
    else          oss << ";10" << static_cast<int> (a.bg - 8);

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

  void terminal_renderer::
  begin_sync_update ()
  {
    // DEC Private Mode 2026: Begin Synchronized Update (BSU).
    //
    // CSI ? 2026 h
    //
    // This tells the terminal to buffer all subsequent output until
    // end_sync_update is called.
    //
    write ("\x1b[?2026h");
  }

  void terminal_renderer::
  end_sync_update ()
  {
    // DEC Private Mode 2026: End Synchronized Update (ESU).
    //
    // CSI ? 2026 l
    //
    // This flushes the buffered output to the screen atomically.
    //
    write ("\x1b[?2026l");
  }
}
