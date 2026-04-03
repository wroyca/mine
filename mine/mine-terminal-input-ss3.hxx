#pragma once

#include <mine/mine-terminal-input.hxx>

namespace mine
{
  // Stateless logic for parsing Single Shift 3 (SS3) sequences.
  //
  // In 7-bit environments (which is effectively 100% of modern usage), SS3
  // is encoded as `ESC O` (0x1B 0x4F).
  //
  // We typically encounter these when the terminal is switched into
  // "Application Cursor Mode" (DECCKM) or "Application Keypad Mode"
  // (DECKPAM). In these modes, keys like Home, End, and the arrows send
  // SS3 sequences instead of the standard CSI sequences to allow full-screen
  // applications to distinguish them from other inputs.
  //
  struct ss3_parser
  {
    // Interpret the byte following the `ESC O` prefix.
    //
    // Unlike CSI sequences, SS3 sequences have a fixed length (ESC O <char>),
    // so we don't need a complex state machine or parameter vector. We just
    // look at the final byte.
    //
    static input_event
    interpret_sequence (char byte);

  private:
    // Map the final byte to a special key.
    //
    // For example, 'P' maps to F1, 'M' to Keypad Enter, etc.
    //
    static special_key
    map_key (char byte);
  };
}
