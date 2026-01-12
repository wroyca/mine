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
  // Note that strictly speaking, we could return a "noop" command for
  // unhandled events, but returning nullptr allows the caller to easily
  // differentiate between "nothing happened" and "command executed but did
  // nothing".
  //
  command_ptr
  make_command (const input_event& e)
  {
    return visit ([] (const auto& x) -> command_ptr
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
        // So let these fall through to the global key binder/shortcut handler
        // which sits above this translator.
        //
        if (has_modifier (x.modifiers, key_modifier::ctrl))
          return nullptr;

        return make_unique<insert_text_command> (x.text);
      }
      // Functional keys.
      //
      else if constexpr (is_same_v<type, special_key_event>)
      {
        switch (x.key)
        {
          // Navigation.
          //
          case special_key::up:    return make_unique<move_cursor_command> (move_direction::up);
          case special_key::down:  return make_unique<move_cursor_command> (move_direction::down);
          case special_key::left:  return make_unique<move_cursor_command> (move_direction::left);
          case special_key::right: return make_unique<move_cursor_command> (move_direction::right);

          // For Home/End we currently map to line start/end.
          //
          // @@: At some point we should differentiate between visual line start
          // (ignoring whitespace) and absolute line start.
          //
          case special_key::home: return make_unique<move_cursor_command> (move_direction::line_start);
          case special_key::end:  return make_unique<move_cursor_command> (move_direction::line_end);

          // Editing.
          //
          case special_key::backspace:  return make_unique<delete_backward_command> ();
          case special_key::delete_key: return make_unique<delete_forward_command> ();
          case special_key::enter:      return make_unique<insert_newline_command> ();

          default: return nullptr;
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
        if (x.button == 64)
          return make_unique<move_cursor_command> (move_direction::scroll_up);
        else if (x.button == 65)
          return make_unique<move_cursor_command> (move_direction::scroll_down);

        return nullptr;
      }
      // Ignore everything else (resize events, unknown variants, etc).
      //
      else
      {
        return nullptr;
      }
    }, e);
  }
}
