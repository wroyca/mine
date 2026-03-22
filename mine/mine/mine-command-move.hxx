#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  // The vocabulary of abstract movement directions.
  //
  // Note that we mix cursor movement (up/down/left/right) with view
  // manipulation (scroll) here. While semantically different, they are usually
  // triggered by the same class of input events (navigation keys/mouse wheel).
  //
  enum class move_direction
  {
    // Relative cursor movement.
    //
    up,
    down,
    left,
    right,

    // Logical line boundaries.
    //
    line_start,   // Home
    line_end,     // End

    // Buffer boundaries.
    //
    buffer_start, // Ctrl+Home
    buffer_end,   // Ctrl+End

    // Viewport manipulation.
    //
    scroll_up,    // Mouse wheel up
    scroll_down   // Mouse wheel down
  };

  // Execute a navigation or scrolling action.
  //
  // This command is read-only regarding the buffer content (it returns false
  // for modifies_buffer), but it produces a new editor state with either an
  // updated cursor position or an updated viewport offset.
  //
  class move_cursor_command: public command
  {
  public:
    explicit
    move_cursor_command (move_direction d, bool select = false)
      : d_ (d),
        select_ (select)
    {
    }

    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Handle viewport scrolling first.
      //
      // Scrolling doesn't move the cursor (the point), it only shifts the
      // view.
      //
      if (d_ == move_direction::scroll_up || d_ == move_direction::scroll_down)
      {
        // TODO: Currently we hardcode the scroll step to 3 lines (standard
        // mouse wheel tick). Ideally, this should come from configuration or
        // the input event itself.
        //
        auto nv (d_ == move_direction::scroll_up
                 ? s.view ().scroll_up (3, b)
                 : s.view ().scroll_down (3, b));

        return s.with_view (std::move (nv));
      }

      // Handle cursor movement.
      //
      // We delegate the heavy lifting of geometry calculations (grapheme
      // boundaries, column memory, line wrapping) to the cursor logic itself.
      //
      auto nc (c);

      if (select_)
      {
        // If the user wants to select text but doesn't have an anchor yet,
        // drop the mark at the pre-move position.
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
        case move_direction::up:           nc = nc.move_up (b);           break;
        case move_direction::down:         nc = nc.move_down (b);         break;
        case move_direction::left:         nc = nc.move_left (b);         break;
        case move_direction::right:        nc = nc.move_right (b);        break;
        case move_direction::line_start:   nc = nc.move_line_start ();    break;
        case move_direction::line_end:     nc = nc.move_line_end (b);     break;
        case move_direction::buffer_start: nc = nc.move_buffer_start ();  break;
        case move_direction::buffer_end:   nc = nc.move_buffer_end (b);   break;
        default: break; // Should be unreachable due to scroll check above.
      }

      // If the move was invalid (e.g., trying to move up from the first line),
      // the cursor logic returns itself unchanged, so this is safe.
      //
      return s.with_cursor (nc);
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "move_cursor";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return false;
    }

  private:
    move_direction d_;
    bool select_;
  };
}
