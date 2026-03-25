#include <mine/mine-command.hxx>

#include <mine/mine-core-view.hxx>

using namespace std;

namespace mine
{
  command::~command () = default;

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
          if (x.text == "q")
            return make_unique<quit_command> ();

          if (x.text == "z")
            return make_unique<undo_command> ();

          if (x.text == "y")
            return make_unique<redo_command> ();

          if (x.text == "c")
            return make_unique<copy_command> ();

          if (x.text == "v")
            return make_unique<paste_command> ();
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
          case special_key::up:
            return make_unique<move_cursor_command> (move_direction::up, s);

          case special_key::down:
            return make_unique<move_cursor_command> (move_direction::down, s);

          case special_key::left:
            return make_unique<move_cursor_command> (move_direction::left, s);

          case special_key::right:
            return make_unique<move_cursor_command> (move_direction::right, s);

          // Editing.
          //
          case special_key::backspace:
            return make_unique<delete_backward_command> ();

          case special_key::delete_key:
            return make_unique<delete_forward_command> ();

          case special_key::enter:
            return make_unique<insert_newline_command> ();

          default:
            break;
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

  mine::editor_state copy_command::
  execute (const editor_state& s) const
  {
    auto c (s.get_cursor ());

    // See if we actually have an active selection. Note that if the mark is
    // at the current position, it doesn't really count.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (std::min (c.position (), c.mark ()));
      auto p2 (std::max (c.position (), c.mark ()));

      // Step the end forward. Cell highlighting is inclusive so we need to
      // grab the character under the cursor too.
      //
      auto e (cursor (p2).move_right (s.buffer ()).position ());

      std::string t (s.buffer ().get_range (p1, e));
      set_clipboard_text (t);

      // Clear the mark. This gives the user visual feedback that the text was
      // actually copied.
      //
      c.clear_mark ();

      return s.with_cursor (c);
    }

    return s;
  }

  std::string_view mine::copy_command::
  name () const noexcept
  {
    return "copy";
  }

  bool mine::copy_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  mine::editor_state mine::paste_command::
  execute (const editor_state& s) const
  {
    // Grab the clipboard text. Bail out early if there is nothing
    // to do.
    //
    std::string t (get_clipboard_text ());
    if (t.empty ())
      return s;

    auto b (s.buffer ());
    auto c (s.get_cursor ());

    // If there is an active selection, pasting should just blast
    // over it.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (std::min (c.position (), c.mark ()));
      auto p2 (std::max (c.position (), c.mark ()));
      auto e (cursor (p2).move_right (b).position ());

      b = b.delete_range (p1, e);
      c = c.move_to (p1);
    }

    c.clear_mark ();

    // Shovel the clipboard text into the buffer. We have to be careful here
    // and handle newlines manually since the text might span multiple lines.
    //
    std::size_t st (0);
    std::size_t p (t.find ('\n'));

    while (p != std::string::npos)
    {
      std::string ck (t.substr (st, p - st));

      if (!ck.empty ())
      {
        b = b.insert_graphemes (c.position (), ck);
        std::size_t n (count_graphemes (ck));

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
      std::string ck (t.substr (st));
      b = b.insert_graphemes (c.position (), ck);
      std::size_t n (count_graphemes (ck));

      c = c.move_to (
        cursor_position (c.line (), column_number (c.column ().value + n)));
    }

    return s.update (std::move (b), c);
  }

  std::string_view mine::paste_command::
  name () const noexcept
  {
    return "paste";
  }

  bool mine::paste_command::
  modifies_buffer () const noexcept
  {
    return true;
  }

  editor_state delete_backward_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
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
  modifies_buffer () const noexcept
  {
    return true;
  }

  editor_state delete_forward_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
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
  modifies_buffer () const noexcept
  {
    return true;
  }

  insert_text_command::
  insert_text_command (string text)
    : text_ (move (text))
  {
  }

  editor_state insert_text_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
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
  modifies_buffer () const noexcept
  {
    return true;
  }

  editor_state insert_newline_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
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
  modifies_buffer () const noexcept
  {
    return true;
  }

  move_cursor_command::
  move_cursor_command (move_direction d, bool select)
    : d_ (d),
      select_ (select)
  {
  }

  editor_state move_cursor_command::
  execute (const editor_state& s) const
  {
    const auto& b (s.buffer ());
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
      case move_direction::up:
        nc = nc.move_up (b);
        break;

      case move_direction::down:
        nc = nc.move_down (b);
        break;

      case move_direction::left:
        nc = nc.move_left (b);
        break;

      case move_direction::right:
        nc = nc.move_right (b);
        break;

      case move_direction::line_start:
        nc = nc.move_line_start ();
        break;

      case move_direction::line_end:
        nc = nc.move_line_end (b);
        break;

      case move_direction::buffer_start:
        nc = nc.move_buffer_start ();
        break;

      case move_direction::buffer_end:
        nc = nc.move_buffer_end (b);
        break;

      default:
        break; // should be unreachable due to scroll check above.
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
  modifies_buffer () const noexcept
  {
    return false;
  }

  editor_state quit_command::
  execute (const editor_state& s) const
  {
    // Meta-command: implementation is bypassed by editor_core::dispatch
    //
    return s;
  }

  string_view quit_command::
  name () const noexcept
  {
    return "quit";
  }

  bool quit_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  editor_state redo_command::
  execute (const editor_state& s) const
  {
    // Meta-command: implementation is bypassed by editor_core::dispatch.
    //
    return s;
  }

  string_view redo_command::
  name () const noexcept
  {
    return "redo";
  }

  bool redo_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  begin_selection_command::
  begin_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  editor_state begin_selection_command::
  execute (const editor_state& s) const
  {
    // Terminals typically report coordinates as (x,y), but our screen
    // position struct expects (row,col), which is (y,x).
    //
    screen_position sp (y_, x_);

    auto p (s.view ().screen_to_buffer (sp, s.buffer ()));

    // Grab the cursor and move it to the resolved position.
    //
    auto c (s.get_cursor ().move_to (p));
    c.set_mark ();

    return s.with_cursor (c);
  }

  string_view begin_selection_command::
  name () const noexcept
  {
    return "begin_selection";
  }

  bool begin_selection_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  update_selection_command::
  update_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  editor_state update_selection_command::
  execute (const editor_state& s) const
  {
    screen_position sp (y_, x_);

    // Again, use the view instance from the state to resolve the drag
    // coordinates.
    //
    auto p (s.view ().screen_to_buffer (sp, s.buffer ()));

    // Move the active cursor point to the new drag coordinates. Since we
    // previously called set_mark() during begin_selection, moving the cursor
    // here automatically expands or shrinks the selected region.
    //
    auto c (s.get_cursor ().move_to (p));

    return s.with_cursor (c);
  }

  string_view update_selection_command::
  name () const noexcept
  {
    return "update_selection";
  }

  bool update_selection_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  // end_selection_command
  //
  end_selection_command::
  end_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  editor_state end_selection_command::
  execute (const editor_state& s) const
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
  modifies_buffer () const noexcept
  {
    return false;
  }

  editor_state undo_command::
  execute (const editor_state& s) const
  {
    // Meta-command: implementation is bypassed by editor_core::dispatch
    //
    return s;
  }

  string_view undo_command::
  name () const noexcept
  {
    return "undo";
  }

  bool undo_command::
  modifies_buffer () const noexcept
  {
    return false;
  }
}
