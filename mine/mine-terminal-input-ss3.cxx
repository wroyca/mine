#include <mine/mine-terminal-input-ss3.hxx>

namespace mine
{
  // Interpret the character following the SS3 prefix (ESC O).
  //
  // SS3 is typically used for "Application" modes. If the terminal is
  // in Application Cursor Mode (DECCKM) or Application Keypad Mode
  // (DECKPAM), it sends these instead of the standard CSI sequences.
  //
  input_event ss3_parser::
  interpret_sequence (char c)
  {
    // SS3 sequences generally don't carry modifiers. If a modifier is
    // held (like Ctrl), modern terminals usually revert to the CSI
    // format (e.g., ESC [ 1 ; 5 A) even if in application mode.
    //
    // So we can safely assume no modifiers here.
    //
    special_key k (map_key (c));
    return special_key_event {k, key_modifier::none};
  }

  special_key ss3_parser::
  map_key (char c)
  {
    switch (c)
    {
      // Cursor keys (DECCKM).
      //
      case 'A': return special_key::up;
      case 'B': return special_key::down;
      case 'C': return special_key::right;
      case 'D': return special_key::left;
      case 'H': return special_key::home;
      case 'F': return special_key::end;

      // Function keys F1-F4.
      //
      // Note that F5 and above usually use the CSI ~ format.
      //
      case 'P': return special_key::f1;
      case 'Q': return special_key::f2;
      case 'R': return special_key::f3;
      case 'S': return special_key::f4;

      // Keypad keys (DECKPAM).
      //
      // We only really care about Enter here. For the digits and operators,
      // we recognize them but map to unknown for now, as we don't strictly
      // need to distinguish "Keypad 5" from "5" in this specific editor
      // context.
      //
      case 'M': return special_key::enter; // Keypad Enter

      case 'j': // *
      case 'k': // +
      case 'l': // ,
      case 'm': // -
      case 'n': // .
      case 'o': // /
      case 'p': // 0
      case 'q': // 1
      case 'r': // 2
      case 's': // 3
      case 't': // 4
      case 'u': // 5
      case 'v': // 6
      case 'w': // 7
      case 'x': // 8
      case 'y': // 9
        return special_key::unknown;

      default:
        return special_key::unknown;
    }
  }
}
