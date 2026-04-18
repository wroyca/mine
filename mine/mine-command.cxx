#include <mine/mine-command.hxx>

#include <mine/mine-viewport.hxx>
#include <algorithm>

using namespace std;

namespace mine
{
  command::~command () = default;

  optional<input_event>
  parse_key_chord (string_view chord)
  {
    key_modifier mods (key_modifier::none);
    string_view key (chord);

    while (true)
    {
      if      (key.starts_with("C-")) { mods = mods | key_modifier::ctrl;  key.remove_prefix(2); }
      else if (key.starts_with("S-")) { mods = mods | key_modifier::shift; key.remove_prefix(2); }
      else if (key.starts_with("A-")) { mods = mods | key_modifier::alt;   key.remove_prefix(2); }
      else if (key.starts_with("M-")) { mods = mods | key_modifier::meta;  key.remove_prefix(2); }
      else break;
    }

    if (key == "up")                          return special_key_event {special_key::up, mods};
    if (key == "down")                        return special_key_event {special_key::down, mods};
    if (key == "left")                        return special_key_event {special_key::left, mods};
    if (key == "right")                       return special_key_event {special_key::right, mods};
    if (key == "enter" || key == "return")    return special_key_event {special_key::enter, mods};
    if (key == "esc"   || key == "escape")    return special_key_event {special_key::escape, mods};
    if (key == "bs"    || key == "backspace") return special_key_event {special_key::backspace, mods};
    if (key == "del"   || key == "delete")    return special_key_event {special_key::delete_key, mods};
    if (key == "tab")                         return special_key_event {special_key::tab, mods};
    if (key == "home")                        return special_key_event {special_key::home, mods};
    if (key == "end")                         return special_key_event {special_key::end, mods};
    if (key == "pageup")                      return special_key_event {special_key::page_up, mods};
    if (key == "pagedown")                    return special_key_event {special_key::page_down, mods};

    if (!key.empty())
      return text_input_event {string(key), mods};

    return nullopt;
  }

  unique_ptr<command>
  make_command_by_name (string_view name)
  {
    if (name == "insert_newline")      return make_unique<insert_newline_command> ();
    if (name == "delete_backward")     return make_unique<delete_backward_command> ();
    if (name == "delete_forward")      return make_unique<delete_forward_command> ();
    if (name == "move_up")             return make_unique<move_cursor_command> (move_direction::up);
    if (name == "move_down")           return make_unique<move_cursor_command> (move_direction::down);
    if (name == "move_left")           return make_unique<move_cursor_command> (move_direction::left);
    if (name == "move_right")          return make_unique<move_cursor_command> (move_direction::right);
    if (name == "move_line_start")     return make_unique<move_cursor_command> (move_direction::line_start);
    if (name == "move_line_end")       return make_unique<move_cursor_command> (move_direction::line_end);
    if (name == "move_buffer_start")   return make_unique<move_cursor_command> (move_direction::buffer_start);
    if (name == "move_buffer_end")     return make_unique<move_cursor_command> (move_direction::buffer_end);
    if (name == "undo")                return make_unique<undo_command> ();
    if (name == "redo")                return make_unique<redo_command> ();
    if (name == "save")                return make_unique<save_command> ();
    if (name == "quit")                return make_unique<quit_command> ();
    if (name == "save_and_quit")       return make_unique<save_and_quit_command> ();
    if (name == "copy")                return make_unique<copy_command> ();
    if (name == "paste")               return make_unique<paste_command> ();
    if (name == "toggle_cmdline")      return make_unique<toggle_cmdline_command> ();
    if (name == "escape")              return make_unique<escape_command> ();
    if (name == "split_horizontal")    return make_unique<split_window_command> (layout_direction::horizontal);
    if (name == "split_vertical")      return make_unique<split_window_command> (layout_direction::vertical);
    if (name == "close_window")        return make_unique<close_window_command> ();
    if (name == "switch_window_up")    return make_unique<switch_window_command> (0, -1);
    if (name == "switch_window_down")  return make_unique<switch_window_command> (0, 1);
    if (name == "switch_window_left")  return make_unique<switch_window_command> (-1, 0);
    if (name == "switch_window_right") return make_unique<switch_window_command> (1, 0);

    if (name.starts_with ("insert_text "))
      return make_unique<insert_text_command> (string (name.substr(12)));

    return nullptr;
  }

  // Map the raw input event (which is a variant of specific event structures)
  // to an abstract command.
  //
  // Note that strictly speaking, we could return a "noop" command for
  // unhandled events, but returning nullptr allows the caller to easily
  // differentiate between "nothing happened" and "command executed but did
  // nothing".
  //
  unique_ptr<command>
  make_command (const input_event& e)
  {
    return visit ([] (const auto& x) -> unique_ptr<command>
    {
      using type = decay_t<decltype (x)>;

      // Text input.
      //
      if constexpr (is_same_v<type, text_input_event>)
      {
        // We treat any text input with the Ctrl modifier as a potential
        // command shortcut (e.g., Ctrl-s for save, Ctrl-q for quit) rather
        // than literal text insertion.
        //
        if (has_modifier (x.modifiers, key_modifier::ctrl))
        {
          if (x.text == "q") return make_unique<quit_command> ();
          if (x.text == "s") return make_unique<save_command> ();
          if (x.text == "z") return make_unique<undo_command> ();
          if (x.text == "y") return make_unique<redo_command> ();
          if (x.text == "c") return make_unique<copy_command> ();
          if (x.text == "v") return make_unique<paste_command> ();
          if (x.text == "p") return make_unique<toggle_cmdline_command> ();
        }

        return make_unique<insert_text_command> (x.text);
      }
      // Functional keys.
      //
      else if constexpr (is_same_v<type, special_key_event>)
      {
        const bool s (has_modifier (x.modifiers, key_modifier::shift));

        switch (x.key)
        {
          // Navigation.
          //
          case special_key::up:         return make_unique<move_cursor_command> (move_direction::up, s);
          case special_key::down:       return make_unique<move_cursor_command> (move_direction::down, s);
          case special_key::left:       return make_unique<move_cursor_command> (move_direction::left, s);
          case special_key::right:      return make_unique<move_cursor_command> (move_direction::right, s);

          // Editing.
          //
          case special_key::backspace:  return make_unique<delete_backward_command> ();
          case special_key::delete_key: return make_unique<delete_forward_command> ();
          case special_key::enter:      return make_unique<insert_newline_command> ();
          case special_key::escape:     return make_unique<escape_command> ();
          default:                      break;
        }
      }
      // Mouse events.
      //
      else if constexpr (is_same_v<type, mouse_event>)
      {
        // Handle scroll wheel.
        //
        // In the XTerm protocol (even with SGR 1006 enabled), the scroll
        // wheel is typically mapped to "Button 4" and "Button 5".
        //
        // Historically, these have bit 6 set (value 64).
        // 64 + 0 = Scroll Up
        // 64 + 1 = Scroll Down
        //
        // @@: We should parameterize the scroll amount later, but for now, a
        // single tick moves a single unit.
        //
        if (x.button == mouse_button::scroll_up)
          return make_unique<move_cursor_command> (move_direction::scroll_up);

        if (x.button == mouse_button::scroll_down)
          return make_unique<move_cursor_command> (move_direction::scroll_down);

        // Handle the left mouse button for text selection.
        //
        // Note that we branch on the state we captured in the SGR parser to
        // determine the phase of the selection.
        //
        if (x.button == mouse_button::left)
        {
          switch (x.state)
          {
            case mouse_state::press:
              // Triggered on the initial left click down.
              //
              return make_unique<begin_selection_command> (x.x, x.y);

            case mouse_state::drag:
              // Triggered as the mouse moves while holding the left click.
              //
              return make_unique<update_selection_command> (x.x, x.y);

            case mouse_state::release:
              // Triggered when the left click is released.
              //
              return make_unique<end_selection_command> (x.x, x.y);

            default:
              break;
          }
        }
      }

      // Ignore everything else (resize events, unknown variants, etc).
      //
      return nullptr;
    }, e);
  }

  // Parse strings directly typed into the command line.
  //
  unique_ptr<command>
  parse_cmdline (string_view action)
  {
    auto start (action.find_first_not_of (" \t"));
    if (start == string_view::npos)
      return nullptr;

    auto end (action.find_last_not_of (" \t"));
    auto trimmed (action.substr (start, end - start + 1));

    if (trimmed == "w" || trimmed == "write")
      return make_unique<save_command> ();

    if (trimmed == "q" || trimmed == "quit")
      return make_unique<close_window_command> (); // Remapped specifically for
                                                   // splits

    if (trimmed == "wq" || trimmed == "x")
      return make_unique<save_and_quit_command> ();

    if (trimmed == "u" || trimmed == "undo")
      return make_unique<undo_command> ();

    if (trimmed == "redo")
      return make_unique<redo_command> ();

    if (trimmed == "sp" || trimmed == "split")
      return make_unique<split_window_command> (layout_direction::horizontal);

    if (trimmed == "vs" || trimmed == "vsplit")
      return make_unique<split_window_command> (layout_direction::vertical);

    return nullptr;
  }

  mine::workspace copy_command::
  execute (const workspace& s) const
  {
    auto c (s.get_cursor ());

    // See if we actually have an active selection. Note that if the mark is
    // at the current position, it doesn't really count.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Step the end forward. Cell highlighting is inclusive so we need to
      // grab the character under the cursor too.
      //
      auto e (cursor (p2).move_right (s.active_content ()).position ());
      string t (s.active_content ().get_range (p1, e));
      set_clipboard_text (t);

      // Clear the mark. This gives the user visual feedback that the text was
      // actually copied.
      //
      c.clear_mark ();
      return s.with_cursor (c);
    }
    return s;
  }

  string_view mine::copy_command::
  name () const noexcept
  {
    return "copy";
  }

  bool mine::copy_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  mine::workspace mine::paste_command::
  execute (const workspace& s) const
  {
    // Grab the clipboard text. Bail out early if there is nothing
    // to do.
    //
    string t (get_clipboard_text ());
    if (t.empty ())
      return s;

    // Paste into cmdline if active.
    //
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());
      erase (t, '\n'); // Prevent multi-line pastes from breaking cmdline.
      cmd.content.insert (cmd.cursor_pos, t);
      cmd.cursor_pos += t.size ();
      return s.with_cmdline (cmd);
    }

    auto b (s.active_content ());
    auto c (s.get_cursor ());

    // If there is an active selection, pasting should just blast
    // over it.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));
      auto e (cursor (p2).move_right (b).position ());

      b = b.delete_range (p1, e);
      c = c.move_to (p1);
    }

    c.clear_mark ();

    // Shovel the clipboard text into the buffer. We have to be careful here
    // and handle newlines manually since the text might span multiple lines.
    //
    size_t st (0);
    size_t p (t.find ('\n'));

    while (p != string::npos)
    {
      string ck (t.substr (st, p - st));

      if (!ck.empty ())
      {
        b = b.insert_graphemes (c.position (), ck);
        size_t n (count_graphemes (ck));

        c = c.move_to (
          cursor_position (c.line (), column_number (c.column ().value + n)));
      }

      b = b.insert_newline (c.position ());
      c = c.move_to (
        cursor_position (line_number (c.line ().value + 1), column_number (0)));

      st = p + 1;
      p = t.find ('\n', st);
    }

    // Don't forget any trailing text after the last newline.
    //
    if (st < t.size ())
    {
      string ck (t.substr (st));
      b = b.insert_graphemes (c.position (), ck);
      size_t n (count_graphemes (ck));

      c = c.move_to (
        cursor_position (c.line (), column_number (c.column ().value + n)));
    }

    return s.update (move (b), c);
  }

  string_view mine::paste_command::
  name () const noexcept
  {
    return "paste";
  }

  bool mine::paste_command::
  modifies_buffer (const workspace& s) const noexcept
  {
    return !s.cmdline ().active;
  }

  workspace delete_backward_command::
  execute (const workspace& s) const
  {
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());
      if (cmd.cursor_pos > 0)
      {
        size_t prev (prev_grapheme_boundary (cmd.content, cmd.cursor_pos));
        cmd.content.erase (prev, cmd.cursor_pos - prev);
        cmd.cursor_pos = prev;
      }
      return s.with_cmdline (cmd);
    }

    auto b (s.active_content ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, backspace simply deletes the
    // marked region.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Note that terminal cell selections are inclusive. So we must step the
      // end boundary forward by one grapheme to make the exclusive delete_range
      // cover it.
      //
      auto e (cursor (p2).move_right (b).position ());

      auto nb (b.delete_range (p1, e));
      auto nc (c.move_to (p1));

      nc.clear_mark ();

      return s.update (move (nb), nc);
    }

    // Clear the empty mark so it doesn't linger and cause trouble later.
    //
    c.clear_mark ();

    // First, try to step back by one grapheme.
    //
    auto nc (c.move_left (b));

    // If the cursor position didn't change, we must be at the very beginning
    // of the buffer. In this case, there is naturally nothing left to delete.
    //
    if (nc.position () == c.position ())
      return s.with_cursor (c);

    // Otherwise, delete the grapheme at the newly computed position.
    //
    // Note that we rely on delete_next_grapheme() to internally take care of
    // the line merging logic if this position happens to point to a newline.
    //
    auto nb (b.delete_next_grapheme (nc.position ()));

    return s.update (move (nb), nc);
  }

  string_view delete_backward_command::
  name () const noexcept
  {
    return "delete_backward";
  }

  bool delete_backward_command::
  modifies_buffer (const workspace& s) const noexcept
  {
    return !s.cmdline ().active;
  }

  workspace delete_forward_command::
  execute (const workspace& s) const
  {
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());
      if (cmd.cursor_pos < cmd.content.size ())
      {
        size_t next (next_grapheme_boundary (cmd.content, cmd.cursor_pos));
        cmd.content.erase (cmd.cursor_pos, next - cmd.cursor_pos);
      }
      return s.with_cmdline (cmd);
    }

    auto b (s.active_content ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, delete simply removes
    // the marked region.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Note that terminal cell selections are inclusive. So we must step the
      // end boundary forward by one grapheme to make the exclusive
      // delete_range cover it.
      //
      auto e (cursor (p2).move_right (b).position ());

      auto nb (b.delete_range (p1, e));
      auto nc (c.move_to (p1));

      nc.clear_mark ();

      return s.update (move (nb), nc);
    }

    c.clear_mark (); // Clear empty mark, if any.
    const auto p (c.position ());

    // See if we are already at the absolute end of the buffer. If so, there is
    // naturally nothing further to delete.
    //
    if (p.line.value >= b.line_count ())
      return s.with_cursor (c);

    // We must also handle the boundary case where we sit exactly past the
    // last character of the final line. For any normal line this situation
    // would trigger a merge with the line below, but for the final line
    // it amounts to a no-op.
    //
    if (p.line.value == b.line_count () - 1 &&
        p.column.value >= b.line_length (p.line))
      return s.with_cursor (c);

    // At this point we can safely delete the next grapheme starting from the
    // current position.
    //
    auto b1 (b.delete_next_grapheme (p));

    return s.update (move (b1), c);
  }

  string_view delete_forward_command::
  name () const noexcept
  {
    return "delete_forward";
  }

  bool delete_forward_command::
  modifies_buffer (const workspace& s) const noexcept
  {
    return !s.cmdline ().active;
  }

  insert_text_command::
  insert_text_command (string text)
    : text_ (move (text))
  {
  }

  workspace insert_text_command::
  execute (const workspace& s) const
  {
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());
      cmd.content.insert (cmd.cursor_pos, text_);
      cmd.cursor_pos += text_.size ();
      return s.with_cmdline (cmd);
    }

    auto b (s.active_content ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, typing replaces the marked
    // region. First, we clear out the selected text.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Step the end boundary forward to account for inclusive selection.
      //
      auto e (cursor (p2).move_right (b).position ());

      b = b.delete_range (p1, e);
      c = c.move_to (p1);
    }

    c.clear_mark ();

    // Sanity check: verify the input layer gave us valid UTF-8. It really
    // should have, but better safe than sorry before we commit to the buffer.
    //
    assert_valid_utf8 (text_);

    // Now inject the grapheme sequence into the buffer right at the current
    // cursor position.
    //
    b = b.insert_graphemes (c.position (), text_);

    // Figure out where the cursor ends up.
    //
    // Since we assume the text contains no newlines (that is, it's strictly a
    // horizontal insertion), we don't need to recalculate the line. We can
    // simply advance the column index by the number of graphemes we just
    // inserted.
    //
    const size_t n (count_graphemes (text_));

    c = c.move_to (
      cursor_position (c.line (), column_number (c.column ().value + n)));

    return s.update (move (b), c);
  }

  string_view insert_text_command::
  name () const noexcept
  {
    return "insert_text";
  }

  bool insert_text_command::
  modifies_buffer (const workspace& s) const noexcept
  {
    return !s.cmdline ().active;
  }

  workspace insert_newline_command::
  execute (const workspace& s) const
  {
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());
      cmd.is_submitted = true;
      return s.with_cmdline (cmd);
    }

    auto b (s.active_content ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, hitting enter replaces the
    // marked region. First, we drop the existing selection.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Step the end boundary forward to account for inclusive selection.
      //
      auto e (cursor (p2).move_right (b).position ());

      b = b.delete_range (p1, e);
      c = c.move_to (p1);
    }

    c.clear_mark ();

    // Now actually split the line in the buffer right at our current
    // cursor position.
    //
    b = b.insert_newline (c.position ());

    // Finally, advance the cursor down to the start of the next line.
    //
    c = c.move_to (
      cursor_position (line_number (c.line ().value + 1), column_number (0)));

    return s.update (move (b), c);
  }

  string_view insert_newline_command::
  name () const noexcept
  {
    return "insert_newline";
  }

  bool insert_newline_command::
  modifies_buffer (const workspace& s) const noexcept
  {
    return !s.cmdline ().active;
  }

  move_cursor_command::
  move_cursor_command (move_direction d, bool select)
    : d_ (d),
      select_ (select)
  {
  }

  workspace move_cursor_command::
  execute (const workspace& s) const
  {
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());

      switch (d_)
      {
        case move_direction::left:
          cmd.cursor_pos = prev_grapheme_boundary (cmd.content, cmd.cursor_pos);
          break;
        case move_direction::right:
          cmd.cursor_pos = next_grapheme_boundary (cmd.content, cmd.cursor_pos);
          break;
        case move_direction::line_start:
        case move_direction::buffer_start:
          cmd.cursor_pos = 0;
          break;
        case move_direction::line_end:
        case move_direction::buffer_end:
          cmd.cursor_pos = cmd.content.size ();
          break;
        default:
          break;
      }
      return s.with_cmdline (cmd);
    }

    const auto& b (s.active_content ());
    const auto& c (s.get_cursor ());

    // Handle viewport scrolling first.
    //
    // Scrolling doesn't move the cursor (the point), it only shifts the view.
    //
    if (d_ == move_direction::scroll_up || d_ == move_direction::scroll_down)
    {
      // TODO: Currently we hardcode the scroll step to 3 lines (standard mouse
      // wheel tick). Ideally, this should come from configuration or the input
      // event itself.
      //
      auto nv (d_ == move_direction::scroll_up ? s.view ().scroll_up (3, b)
                                               : s.view ().scroll_down (3, b));
      return s.with_view (move (nv));
    }

    // Handle cursor movement.
    //
    // We delegate the heavy lifting of geometry calculations (grapheme
    // boundaries, column memory, line wrapping) to the cursor logic itself.
    //
    auto nc (c);

    if (select_)
    {
      // If the user wants to select text but doesn't have an anchor yet, drop
      // the mark at the pre-move position.
      //
      if (!nc.has_mark ())
        nc.set_mark ();
    }
    else
    {
      // Regular unshifted movement always clears the active selection.
      //
      nc.clear_mark ();
    }

    switch (d_)
    {
      case move_direction::up:           nc = nc.move_up (b);          break;
      case move_direction::down:         nc = nc.move_down (b);        break;
      case move_direction::left:         nc = nc.move_left (b);        break;
      case move_direction::right:        nc = nc.move_right (b);       break;
      case move_direction::line_start:   nc = nc.move_line_start ();   break;
      case move_direction::line_end:     nc = nc.move_line_end (b);    break;
      case move_direction::buffer_start: nc = nc.move_buffer_start (); break;
      case move_direction::buffer_end:   nc = nc.move_buffer_end (b);  break;
      default: break;
    }

    // If the move was invalid (e.g., trying to move up from the first line),
    // the cursor logic returns itself unchanged, so this is safe.
    //
    return s.with_cursor (nc);
  }

  string_view move_cursor_command::
  name () const noexcept
  {
    return "move_cursor";
  }

  bool move_cursor_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace save_command::
  execute (const workspace& s) const
  {
    return s;
  }

  string_view save_command::
  name () const noexcept
  {
    return "save";
  }

  bool save_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace save_and_quit_command::
  execute (const workspace& s) const
  {
    return s;
  }

  string_view save_and_quit_command::
  name () const noexcept
  {
    return "save_and_quit";
  }

  bool save_and_quit_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace quit_command::
  execute (const workspace& s) const
  {
    // Meta-command: implementation is bypassed by editor::dispatch
    //
    return s;
  }

  string_view quit_command::
  name () const noexcept
  {
    return "quit";
  }

  bool quit_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace redo_command::
  execute (const workspace& s) const
  {
    // Meta-command: implementation is bypassed by editor::dispatch.
    //
    return s;
  }

  string_view redo_command::
  name () const noexcept
  {
    return "redo";
  }

  bool redo_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  begin_selection_command::
  begin_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  workspace begin_selection_command::
  execute (const workspace& s) const
  {
    vector<window_partition> lays;
    s.get_layout (lays,
                  s.global_size ().cols,
                  s.global_size ().rows > 0 ? s.global_size ().rows - 1 : 0);

    window_id hit (s.active_window ());
    const window_partition* lay (nullptr);

    // Resolve which specific split physically received the mouse click so
    // we accurately map the coordinate space into the corresponding buffer.
    //
    for (const auto& l : lays)
    {
      if (x_ >= l.x && x_ < l.x + l.w && y_ >= l.y && y_ < l.y + l.h)
      {
        hit = l.win;
        lay = &l;
        break;
      }
    }

    auto ns (s.switch_window_direct (hit));

    if (!lay)
    {
      for (const auto& l : lays)
      {
        if (l.win == hit)
        {
          lay = &l;
          break;
        }
      }
    }

    if (lay)
    {
      screen_position sp (y_ - lay->y, x_ - lay->x);
      auto p (ns.view ().screen_to_buffer (sp, ns.active_content ()));
      auto c (ns.get_cursor ().move_to (p));
      c.set_mark ();
      return ns.with_cursor (c);
    }

    return ns;
  }

  string_view begin_selection_command::
  name () const noexcept
  {
    return "begin_selection";
  }

  bool begin_selection_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  update_selection_command::
  update_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  workspace update_selection_command::
  execute (const workspace& s) const
  {
    vector<window_partition> lays;
    s.get_layout (lays,
                  s.global_size ().cols,
                  s.global_size ().rows > 0 ? s.global_size ().rows - 1 : 0);

    const window_partition* lay (nullptr);

    for (const auto& l : lays)
    {
      if (l.win == s.active_window ())
      {
        lay = &l;
        break;
      }
    }

    if (lay)
    {
      screen_position sp (y_ > lay->y ? y_ - lay->y : 0,
                          x_ > lay->x ? x_ - lay->x : 0);

      auto p (s.view ().screen_to_buffer (sp, s.active_content ()));
      auto c (s.get_cursor ().move_to (p));
      return s.with_cursor (c);
    }

    return s;
  }

  string_view update_selection_command::
  name () const noexcept
  {
    return "update_selection";
  }

  bool update_selection_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  end_selection_command::
  end_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  workspace end_selection_command::
  execute (const workspace& s) const
  {
    // For many editors, releasing the mouse button doesn't actually change the
    // internal cursor state; the selection remains active until a keyboard
    // event clears it. If the cursor strayed outside the viewport bounds during
    // a drag, we might do a final clamp here. We could also hook system
    // clipboard integration here if desired. For now, we simply return the
    // state untouched.
    //
    return s;
  }

  string_view end_selection_command::
  name () const noexcept
  {
    return "end_selection";
  }

  bool end_selection_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace undo_command::
  execute (const workspace& s) const
  {
    // Meta-command: implementation is bypassed by editor::dispatch
    //
    return s;
  }

  string_view undo_command::
  name () const noexcept
  {
    return "undo";
  }

  bool undo_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace toggle_cmdline_command::
  execute (const workspace& s) const
  {
    auto cmd (s.cmdline ());
    cmd.active = !cmd.active;

    if (cmd.active)
    {
      cmd.content.clear ();
      cmd.cursor_pos = 0;
    }

    return s.with_cmdline (cmd);
  }

  string_view toggle_cmdline_command::
  name () const noexcept
  {
    return "toggle_cmdline";
  }

  bool toggle_cmdline_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace escape_command::
  execute (const workspace& s) const
  {
    // If the cmdline is active, pressing escape cancels it and clears it out.
    //
    if (s.cmdline ().active)
    {
      auto cmd (s.cmdline ());
      cmd.active = false;
      cmd.content.clear ();
      cmd.cursor_pos = 0;
      return s.with_cmdline (cmd);
    }

    // Otherwise, normal escape behavior (e.g., clear the selection anchor).
    //
    auto cur (s.get_cursor ());
    cur.clear_mark ();
    return s.with_cursor (cur);
  }

  string_view escape_command::
  name () const noexcept
  {
    return "escape";
  }

  bool escape_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  split_window_command::
  split_window_command (layout_direction d)
    : d_ (d)
  {
  }

  workspace split_window_command::
  execute (const workspace& s) const
  {
    return s.split_active_window (d_);
  }

  string_view split_window_command::
  name () const noexcept
  {
    return "split_window";
  }

  bool split_window_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  switch_window_command::
  switch_window_command (int dx, int dy)
    : dx_ (dx),
      dy_ (dy)
  {
  }

  workspace switch_window_command::
  execute (const workspace& s) const
  {
    return s.switch_window (dx_, dy_);
  }

  string_view switch_window_command::
  name () const noexcept
  {
    return "switch_window";
  }

  bool switch_window_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }

  workspace close_window_command::
  execute (const workspace& s) const
  {
    return s.close_active_window ();
  }

  string_view close_window_command::
  name () const noexcept
  {
    return "close_window";
  }

  bool close_window_command::
  modifies_buffer (const workspace&) const noexcept
  {
    return false;
  }
}
