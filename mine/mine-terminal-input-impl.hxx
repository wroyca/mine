#pragma once

#include <cctype>

#include <mine/mine-terminal-input.hxx>
#include <mine/mine-terminal-input-csi.hxx>
#include <mine/mine-terminal-input-ss3.hxx>

namespace mine
{
  template <typename F>
  void terminal_input_parser::
  process_normal (char c, F&& cb)
  {
    // The "ground" state. We are mostly looking for printable UTF-8 characters
    // here, but we also need to catch the Escape key to switch modes, and a
    // handful of legacy C0 control codes that we care about (Enter, Tab,
    // Backspace).
    //

    // Escape (0x1B).
    //
    // This is the start of every control sequence.
    //
    if (c == '\x1b')
    {
      // If we were in the middle of decoding a multi-byte UTF-8 character
      // and suddenly got an ESC, the stream is garbage. Reset the decoder.
      //
      utf8_.reset ();

      // Switch to escape sequence collection mode.
      //
      buffer_.clear ();
      state_ = state::escape;
      return;
    }

    // C0 Control Codes.
    //
    // These are the single-byte codes that actually mean something for
    // editing.
    //
    if (c == '\x7f') // DEL: The standard Backspace on modern systems.
    {
      utf8_.reset ();
      cb (special_key_event {special_key::backspace, key_modifier::none});
    }
    else if (c == '\r' || c == '\n')
    {
      // Normalize CR, LF, and CRLF (implicit) to a single Enter event.
      //
      utf8_.reset ();
      cb (special_key_event {special_key::enter, key_modifier::none});
    }
    else if (c == '\t')
    {
      utf8_.reset ();
      cb (special_key_event {special_key::tab, key_modifier::none});
    }
    else if (std::iscntrl (static_cast<unsigned char> (c)))
    {
      // Legacy Ctrl+Key mapping.
      //
      // In ASCII, Ctrl+A is 1, Ctrl+B is 2, etc. We map these back to explicit
      // events so the upper layers don't have to deal with raw ASCII values.
      //
      utf8_.reset ();

      if (c >= 1 && c <= 26)
      {
        char l (c + 'a' - 1);
        std::string s (1, l);
        cb (text_input_event {std::move (s), key_modifier::ctrl});
      }

      // We explicitly ignore other C0 codes (BELL 0x07, NULL 0x00, etc.) as
      // they are noise in an input stream.
      //
    }
    else
    {
      // UTF-8 Text Processing.
      //
      // You might wonder: "Why do we need a stateful UTF-8 decoder here?
      // Can't we just forward the raw bytes to the buffer and let the
      // storage layer sort them out?"
      //
      // The answer is Atomicity and Event Validity.
      //
      // Terminals send data as a stream of bytes. If the user types a
      // character like 'é' (U+00E9), the terminal transmits two distinct
      // bytes: 0xC3 followed by 0xA9.
      //
      // If we were to emit a `text_input_event` for 0xC3 immediately, the
      // upper layers of the editor would receive an invalid, partial
      // string. This causes cascading issues:
      //
      //   a. The editor might try to display this partial state, resulting
      //      in "replacement character" () flickering.
      //   b. The undo/redo history might record "insert 0xC3" as a
      //      distinct action, meaning the user has to hit Undo twice to
      //      remove one character.
      //   c. Key bindings relying on specific characters would fail or
      //      behave unpredictably.
      //
      // Therefore, we must act as a barrier. We accumulate bytes here and
      // only fire an event when we have a semantically complete Unit of
      // Text (a valid codepoint).
      //
      std::string s (utf8_.process (static_cast<unsigned char> (c)));

      if (!s.empty ())
        // Full grapheme/codepoint assembled.
        //
        cb (text_input_event {std::move (s), key_modifier::none});
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_csi (char c, F&& cb)
  {
    // ECMA-48 "Control Sequence Introducer" (CSI) parser.
    //
    // Structure: ESC [ <Parameter Bytes> <Intermediate Bytes> <Final Byte>
    //
    // Ranges:
    //   Parameter:    0x30-0x3F (0-9, ;, <, =, >, ?)
    //   Intermediate: 0x20-0x2F (space, !, ", #, $, %, &, ', (, ), *, +, ,, -, ., /)
    //   Final:        0x40-0x7E (@, A-Z, [, \, ], ^, _, `, a-z, {, |, }, ~)
    //

    // Is this a Final Byte?
    //
    if (c >= 0x40 && c <= 0x7E)
    {
      // We found the terminator. Now we need to dissect the buffer we've
      // collected so far into parameters and intermediates.
      //
      // According to the standard, intermediates (if any) always appear
      // *after* parameters.
      //
      std::size_t n (buffer_.size ());
      std::size_t split (n);

      for (std::size_t i (0); i < n; ++i)
      {
        if (buffer_[i] >= 0x20 && buffer_[i] <= 0x2F)
        {
          split = i;
          break;
        }
      }

      // Slice the buffer.
      //
      auto p (buffer_.substr (0, split)); // Parameters
      auto i (buffer_.substr (split));    // Intermediates

      // Parse the integer parameters (e.g., "1;2" -> {1, 2}).
      //
      auto v (csi_parser::parse_params (p));

      // Check for private mode prefixes (like '?' in "?1049h").
      // These technically sit in the parameter range but change the meaning
      // of the command entirely.
      //
      char prefix (0);
      if (!p.empty ())
      {
        char h (p[0]);
        if (h == '<' || h == '?' || h == '>')
          prefix = h;
      }

      // Interpret and dispatch.
      //
      auto ev (csi_parser::interpret_sequence (c, v, i, prefix));
      cb (ev);

      // Reset to ground state.
      //
      buffer_.clear ();
      state_ = state::normal;
    }
    // Is this a valid Parameter or Intermediate byte?
    //
    else if ((c >= 0x20 && c <= 0x2F) || (c >= 0x30 && c <= 0x3F))
    {
      buffer_.push_back (c);

      // Safety: Prevent buffer overflow attacks or stuck states.
      //
      // Realistically, no CSI sequence should exceed ~20 bytes (even with
      // complicated SGR colors). 64 is generous.
      //
      if (buffer_.size () > 64)
        state_ = state::normal;
    }
    else
    {
      // Invalid byte for a CSI sequence. Abort and return to normal to
      // avoid eating the rest of the stream.
      //
      state_ = state::normal;
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_ss3 (char c, F&& cb)
  {
    // Single Shift 3 (SS3).
    //
    // Sequence: ESC O <Final Byte>
    //
    // This is much simpler than CSI. It's strictly fixed length (one char
    // payload). Used mostly for F1-F4 and cursor keys in Application Mode.
    //
    cb (ss3_parser::interpret_sequence (c));
    state_ = state::normal;
  }
}
