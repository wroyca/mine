#include <mine/mine-command-base.hxx>

#include <mine/mine-command-clipboard.hxx>
#include <mine/mine-command-delete.hxx>
#include <mine/mine-command-insert.hxx>
#include <mine/mine-command-move.hxx>
#include <mine/mine-command-quit.hxx>
#include <mine/mine-command-redo.hxx>
#include <mine/mine-command-selection.hxx>
#include <mine/mine-command-undo.hxx>

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
}
