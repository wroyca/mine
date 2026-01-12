#include <mine/mine-command-base.hxx>

#include <mine/mine-command-move.hxx>
#include <mine/mine-command-insert.hxx>
#include <mine/mine-command-delete.hxx>

using namespace std;

namespace mine
{
  // Map the raw input event (which is a variant of specific event structures)
  // to an abstract command.
  //
  // Note that we return nullptr for events that don't map to a command (like
  // resizing or unknown keys). The caller is expected to handle or ignore
  // nulls.
  //
  command_ptr
  make_command (const input_event& e)
  {
    return visit ([] (auto&& x) -> command_ptr
    {
      using type = decay_t<decltype (x)>;

      // Regular typing.
      //
      if constexpr (is_same_v<type, key_press_event>)
      {
        return make_unique<insert_char_command> (x.ch);
      }
      // Functional keys.
      //
      else if constexpr (is_same_v<type, special_key_event>)
      {
        switch (x.key)
        {
          // Navigation.
          //
          case special_key::up:
            return make_unique<move_cursor_command> (move_direction::up);
          case special_key::down:
            return make_unique<move_cursor_command> (move_direction::down);
          case special_key::left:
            return make_unique<move_cursor_command> (move_direction::left);
          case special_key::right:
            return make_unique<move_cursor_command> (move_direction::right);

          case special_key::home:
            return make_unique<move_cursor_command> (move_direction::line_start);
          case special_key::end:
            return make_unique<move_cursor_command> (move_direction::line_end);

          // Editing.
          //
          case special_key::backspace:
            return make_unique<delete_backward_command> ();
          case special_key::delete_key:
            return make_unique<delete_forward_command> ();
          case special_key::enter:
            return make_unique<insert_newline_command> ();

          default:
            return nullptr;
        }
      }
      // Mouse events.
      //
      else if constexpr (is_same_v<type, mouse_event>)
      {
        // Handle scroll wheel.
        //
        // In the XTerm protocol (even with SGR 1006 enabled), the scroll wheel
        // is mapped to "Button 4" and "Button 5".
        //
        // Bit 6 (value 64) is set for these buttons.
        // 64 + 0 = Scroll Up
        // 64 + 1 = Scroll Down
        //
        if (x.button == 64)
          return make_unique<move_cursor_command> (move_direction::scroll_up);
        else if (x.button == 65)
          return make_unique<move_cursor_command> (move_direction::scroll_down);

        return nullptr;
      }
      // Ignore everything else (resize events, etc).
      //
      else
      {
        return nullptr;
      }
    }, e);
  }
}
