#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  // The vocabulary of movement.
  //
  enum class move_direction
  {
    up,
    down,
    left,
    right,
    line_start,   // Home
    line_end,     // End
    buffer_start, // Ctrl+Home
    buffer_end,   // Ctrl+End
    scroll_up,    // Mouse wheel up
    scroll_down   // Mouse wheel down
  };

  // The navigator.
  //
  // Unlike insert/delete, this command is non-destructive. It generates a new
  // state with a new cursor position, but shares the existing text buffer
  // structure.
  //
  class move_cursor_command: public command
  {
  public:
    explicit
    move_cursor_command (move_direction d)
      : d_ (d)
    {
    }

    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Delegate the actual geometry calculations to the cursor.
      //
      auto nc (c);

      switch (d_)
      {
        case move_direction::up:
          nc = c.move_up (b);
          break;
        case move_direction::down:
          nc = c.move_down (b);
          break;
        case move_direction::left:
          nc = c.move_left (b);
          break;
        case move_direction::right:
          nc = c.move_right (b);
          break;
        case move_direction::line_start:
          nc = c.move_line_start ();
          break;
        case move_direction::line_end:
          nc = c.move_line_end (b);
          break;
        case move_direction::buffer_start:
          nc = c.move_buffer_start ();
          break;
        case move_direction::buffer_end:
          nc = c.move_buffer_end (b);
          break;
        case move_direction::scroll_up:
          {
            auto nv (s.view ().scroll_up (3, b));
            return s.with_view (nv);
          }
        case move_direction::scroll_down:
          {
            auto nv (s.view ().scroll_down (3, b));
            return s.with_view (nv);
          }
      }

      // If the move was invalid (e.g., up from line 0), the cursor returns
      // itself unchanged.
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
  };
}
