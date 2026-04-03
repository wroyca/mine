#pragma once

#include <vector>
#include <string_view>
#include <cstdint>

#include <mine/mine-terminal-input.hxx>

namespace mine
{
  // Stateless logic for parsing Control Sequence Introducer (CSI) sequences.
  //
  // These are the escape codes starting with `ESC [` (0x1B 0x5B).
  //
  // The structure, per ECMA-48 / XTerm, is:
  //   CSI [ P ... ] [ I ... ] F
  //
  // Where:
  //   P (Parameters)    : 0x30-0x3F ('0'-'9', ';', etc)
  //   I (Intermediates) : 0x20-0x2F (space, '!', '?', etc)
  //   F (Final)         : 0x40-0x7E ('@'-'~')
  //
  // Note that we don't maintain the parsing state machine (that lives in the
  // main terminal_input class); rather, we act as the translator once we
  // have collected a complete sequence buffer.
  //
  struct csi_parser
  {
    // Break down the parameter string (e.g., "1;15;0") into integers.
    //
    // We treat empty slots (e.g., "1;;3") as default values (usually 0),
    // consistent with standard terminal behavior.
    //
    static std::vector<int>
    parse_params (std::string_view s);

    // Convert a fully parsed CSI sequence into a high-level input event.
    //
    // We need the final byte to determine the command type (e.g., 'A' for
    // Up Arrow, '~' for Function Key), the parameters for modifiers or
    // key codes, and the intermediates for differentiating standards (like
    // the SGR mouse mode which uses '<').
    //
    static input_event
    interpret_sequence (char final,
                        const std::vector<int>& params,
                        std::string_view inters,
                        char prefix = 0);

  private:
    // Helper mappings.
    //
    // The cursor keys usually map 1:1 based on the final byte, while function
    // keys (F1-F12, Home, End block) often rely on the first parameter
    // (e.g., "15~" is F5).
    //
    static special_key
    map_cursor_key (char final);

    static special_key
    map_function_key (int param);

    // XTerm-style modifier decoding.
    //
    // Modifiers are often packed into a parameter: Shift=1, Alt=2, Ctrl=4.
    //
    static key_modifier
    extract_modifiers (int param);
  };
}
