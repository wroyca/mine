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
    // The ground state.
    //
    // Most of the time we are just receiving text. But we have to keep an
    // eye out for the escape character (0x1B), which signals the start of
    // a control sequence.
    //
    if (c == '\x1b')
    {
      state_ = state::escape;
      buffer_.clear ();
      return;
    }

    // Handle the classic C0 control codes that actually matter for an editor.
    //
    if (c == '\x7f') // DEL (Backspace on modern terminals)
    {
      cb (special_key_event {special_key::backspace, key_modifier::none});
    }
    else if (c == '\r' || c == '\n')
    {
      // Map both CR and LF to Enter. We don't distinguish between them in
      // input handling to keep things simple.
      //
      cb (special_key_event {special_key::enter, key_modifier::none});
    }
    else if (c == '\t')
    {
      cb (special_key_event {special_key::tab, key_modifier::none});
    }
    else if (std::iscntrl (static_cast<unsigned char> (c)))
    {
      // Legacy Control+Key mapping (ASCII 1-26).
      //
      if (c >= 1 && c <= 26)
      {
        char l (c + 'a' - 1);
        cb (key_press_event {l, key_modifier::ctrl});
      }

      // Ignore other C0 noises (BELL, NULL, etc).
      //
    }
    else
    {
      // Regular text.
      //
      cb (key_press_event {c, key_modifier::none});
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_csi (char c, F&& cb)
  {
    // ECMA-48 CSI structure: ESC [ P... I... F
    //
    // P (Parameters)   : 0x30-0x3F
    // I (Intermediates): 0x20-0x2F
    // F (Final)        : 0x40-0x7E
    //
    if (c >= 0x40 && c <= 0x7E)
    {
      // Found the terminator.
      //
      // We need to separate parameters from intermediates. Intermediates
      // (if any) always appear at the end of the accumulated buffer.
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

      // Delegate the heavy lifting to the helper.
      //
      auto p (buffer_.substr (0, split));
      auto i (buffer_.substr (split));

      auto pv (csi_parser::parse_params (p));

      // Extract prefix character (e.g., '<' for SGR mouse, '?' for private
      // modes).
      //
      char prefix (0);
      if (!p.empty () && (p[0] == '<' || p[0] == '?' || p[0] == '>'))
        prefix = p[0];

      auto ev (csi_parser::interpret_sequence (c, pv, i, prefix));

      cb (ev);

      buffer_.clear ();
      state_ = state::normal;
    }
    else if ((c >= 0x20 && c <= 0x2F) || (c >= 0x30 && c <= 0x3F))
    {
      // Still collecting payload.
      //
      buffer_.push_back (c);

      // Safety valve: If we haven't seen a final byte after 64 chars, this
      // is likely garbage or a memory attack. Reset.
      //
      if (buffer_.size () > 64)
        state_ = state::normal;
    }
    else
    {
      // Illegal byte in CSI state. Abort.
      //
      state_ = state::normal;
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_ss3 (char c, F&& cb)
  {
    // SS3 (ESC O <char>) is strictly fixed-length.
    //
    cb (ss3_parser::interpret_sequence (c));
    state_ = state::normal;
  }
}
