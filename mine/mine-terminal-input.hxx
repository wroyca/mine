#pragma once

#include <cctype>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <mine/mine-types.hxx>

namespace mine
{
  // Key modifiers.
  //
  // We use a strongly typed bitmask here. It's safer than raw ints and allows
  // us to define strict operator overloads so we don't accidentally mix
  // modifiers with other flags.
  //
  enum class key_modifier : std::uint8_t
  {
    none  = 0,
    shift = 1,
    alt   = 2,
    ctrl  = 4,
    meta  = 8
  };

  constexpr key_modifier
  operator| (key_modifier a, key_modifier b) noexcept
  {
    return static_cast<key_modifier> (
      static_cast<std::uint8_t> (a) | static_cast<std::uint8_t> (b));
  }

  constexpr key_modifier
  operator& (key_modifier a, key_modifier b) noexcept
  {
    return static_cast<key_modifier> (
      static_cast<std::uint8_t> (a) & static_cast<std::uint8_t> (b));
  }

  constexpr bool
  has_modifier (key_modifier mods, key_modifier mod) noexcept
  {
    return (mods & mod) != key_modifier::none;
  }

  // The vocabulary of non-text keys.
  //
  // We explicitly include keys like 'Enter', 'Tab', and 'Backspace' here,
  // separating them from the text stream.
  //
  // Why? Because in a text editor, these are almost always *commands* rather
  // than *content*.
  //
  enum class special_key
  {
    // Directional pad.
    //
    up, down, left, right,
    home, end,
    page_up, page_down,

    // Editing primitives.
    //
    backspace, delete_key,
    insert,
    tab,
    enter,
    escape,

    // Function keys.
    //
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,

    // Catch-all.
    //
    unknown
  };

  // Events
  //

  // Validated UTF-8 text input.
  //
  // This event guarantees that `text` is a valid, complete UTF-8 sequence
  // representing at least one grapheme cluster. Partial sequences never
  // bubble up to this level.
  //
  struct text_input_event
  {
    std::string  text;
    key_modifier modifiers;

    auto operator<=> (const text_input_event&) const = default;
  };

  // Non-printable control key press.
  //
  struct special_key_event
  {
    special_key  key;
    key_modifier modifiers;

    auto operator<=> (const special_key_event&) const = default;
  };

  enum class mouse_button : uint8_t
  {
    left        = 0,
    middle      = 1,
    right       = 2,
    scroll_up   = 64,
    scroll_down = 65
  };

  // Mouse state during the event.
  //
  enum class mouse_state : std::uint8_t
  {
    press,
    release,
    drag
  };

  // Mouse interaction (via XTerm SGR protocol).
  //
  struct mouse_event
  {
    std::uint16_t x;
    std::uint16_t y;
    mouse_button  button;
    key_modifier  modifiers;
    mouse_state   state;

    auto operator<=> (const mouse_event&) const = default;
  };

  // Terminal window resize notification.
  //
  struct resize_event
  {
    screen_size new_size;

    auto operator<=> (const resize_event&) const = default;
  };

  // The unified input stream type.
  //
  using input_event = std::variant<text_input_event,
                                   special_key_event,
                                   mouse_event,
                                   resize_event>;

  // Infrastructure
  //

  // Streaming UTF-8 Decoder.
  //
  // This class solves the "fragmentation problem". TTYs are streams, and a
  // multi-byte character like '€' (3 bytes) might arrive in two separate
  // `read()` calls: first the header, then the payload.
  //
  // We must not emit a partial character. This class buffers bytes until a
  // valid sequence is complete.
  //
  struct utf8_decoder
  {
    std::string buffer;      // Accumulator.
    int         expect {0};  // How many total bytes we need.
    int         seen   {0};  // How many we have.

    // Feed a byte. Returns a string if a full sequence is completed,
    // otherwise returns empty.
    //
    std::string
    process (unsigned char b)
    {
      // Check if we are starting a new sequence.
      //
      if (expect == 0)
      {
        if ((b & 0x80) == 0)
        {
          // ASCII (0xxxxxxx) is a single byte.
          //
          return std::string (1, static_cast<char> (b));
        }
        else if ((b & 0xE0) == 0xC0) // 110xxxxx (2 bytes)
        {
          expect = 2;
          seen = 1;
          buffer.clear ();
          buffer.push_back (static_cast<char> (b));
        }
        else if ((b & 0xF0) == 0xE0) // 1110xxxx (3 bytes)
        {
          expect = 3;
          seen = 1;
          buffer.clear ();
          buffer.push_back (static_cast<char> (b));
        }
        else if ((b & 0xF8) == 0xF0) // 11110xxx (4 bytes)
        {
          expect = 4;
          seen = 1;
          buffer.clear ();
          buffer.push_back (static_cast<char> (b));
        }
        else
        {
          // Invalid start byte (continuation byte alone or 0xFF). Return
          // the Replacement Character U+FFFD.
          //
          return "\xEF\xBF\xBD";
        }
      }
      else
      {
        // Continuation byte (10xxxxxx).
        //
        if ((b & 0xC0) == 0x80)
        {
          buffer.push_back (static_cast<char> (b));
          seen++;

          if (seen == expect)
          {
            // The sequence is complete. Move out the buffer.
            //
            std::string r (std::move (buffer));
            buffer.clear ();
            expect = 0;
            seen = 0;
            return r;
          }
        }
        else
        {
          // Synchronization error. We expected a continuation byte but got
          // something else. Reset the state and emit a replacement.
          //
          buffer.clear ();
          expect = 0;
          seen = 0;
          return "\xEF\xBF\xBD";
        }
      }

      // Keep feeding bytes.
      //
      return {};
    }

    void
    reset ()
    {
      buffer.clear ();
      expect = 0;
      seen = 0;
    }
  };

  // Stateless logic for parsing Control Sequence Introducer (CSI) sequences.
  //
  // These are the escape codes starting with ESC [ (0x1B 0x5B). The structure,
  // per ECMA-48 and XTerm, is CSI [ P ... ] [ I ... ] F. Note that we do not
  // maintain the parsing state machine here; rather, we act as the translator
  // once we have collected a complete sequence buffer.
  //
  struct csi_parser
  {
    // Break down the parameter string into integers. We treat empty slots
    // as default values (usually 0), which is consistent with standard
    // terminal behavior.
    //
    static std::vector<int>
    parse_params (std::string_view s);

    // Convert a fully parsed CSI sequence into a high-level input event.
    // We need the final byte to determine the command type (for example,
    // 'A' for Up Arrow), the parameters for modifiers, and the intermediates
    // to differentiate standards.
    //
    static input_event
    interpret_sequence (char final,
                        const std::vector<int>& params,
                        std::string_view inters,
                        char prefix = 0);

  private:
    // Helper mappings. The cursor keys typically map 1:1 based on the final
    // byte, while function keys often rely on the first parameter.
    //
    static special_key
    map_cursor_key (char final);

    static special_key
    map_function_key (int param);

    // XTerm-style modifier decoding. Modifiers are often packed into a
    // parameter: Shift=1, Alt=2, Ctrl=4.
    //
    static key_modifier
    extract_modifiers (int param);
  };

  // Stateless logic for parsing Single Shift 3 (SS3) sequences.
  //
  // In 7-bit environments, SS3 is encoded as ESC O (0x1B 0x4F). We usually
  // encounter these when the terminal is switched into Application Cursor Mode
  // (DECCKM) or Application Keypad Mode (DECKPAM).
  //
  struct ss3_parser
  {
    // Interpret the byte following the ESC O prefix. Since SS3 sequences
    // have a fixed length, we just look at the final byte.
    //
    static input_event
    interpret_sequence (char byte);

  private:
    // Map the final byte to a special key.
    //
    static special_key
    map_key (char byte);
  };

  // The main terminal input parser.
  //
  // Essentially, this is a state machine that sits between the raw TTY read()
  // loop and the application's event loop.
  //
  class terminal_input_parser
  {
  public:
    terminal_input_parser () = default;

    // Pump raw bytes into the parser.
    //
    // The callback will be invoked zero or more times with an input_event.
    // We use a template callback to avoid heap-allocating a vector for every
    // tiny keypress.
    //
    template <typename F>
    void
    parse (const char* data, std::size_t size, F&& cb)
    {
      for (std::size_t i (0); i < size; ++i)
        process_byte (data[i], cb);
    }

    // Return true if the machine is strictly idle. If this returns false
    // after a read timeout, it usually means we have a stranded escape
    // sequence (for example, the user pressed ESC and waited).
    //
    bool
    is_clean () const noexcept
    {
      return state_ == state::normal && buffer_.empty ();
    }

    void
    reset () noexcept
    {
      state_ = state::normal;
      buffer_.clear ();
    }

  private:
    enum class state
    {
      normal,      // Passing through text.
      escape,      // Saw '\e'.
      csi,         // Saw '\e[' (Control Sequence).
      ss3,         // Saw '\eO' (Single Shift).
      osc,         // Saw '\e]' (OS Command).
      osc_string   // Inside OSC payload.
    };

    state state_ {state::normal};

    std::string  buffer_; // Payload accumulator.
    utf8_decoder utf8_;

    // Dispatcher.
    //
    template <typename F>
    void
    process_byte (char b, F&& cb)
    {
      switch (state_)
      {
        case state::normal:     process_normal (b, cb); break;
        case state::escape:     process_escape (b, cb); break;
        case state::csi:        process_csi    (b, cb); break;
        case state::ss3:        process_ss3    (b, cb); break;
        case state::osc:
        case state::osc_string: process_osc    (b, cb); break;
      }
    }

    // State handlers.
    //
    template <typename F> void process_normal (char b, F&& cb);
    template <typename F> void process_escape (char b, F&& cb);
    template <typename F> void process_csi    (char b, F&& cb);
    template <typename F> void process_ss3    (char b, F&& cb);

    // OSC is rare in editors (mostly setting clipboard/title), so we handle
    // it inline here to keep the header cleaner.
    //
    template <typename F>
    void
    process_osc (char b, F&& /*cb*/)
    {
      // The format is ESC ] <cmd> ; <payload> (BEL | ST) where ST is ESC \.
      //
      buffer_.push_back (b);

      bool term (false);

      if (b == '\x07') // BEL
      {
        term = true;
      }
      else if (state_ == state::osc_string &&
               b == '\\' &&
               buffer_.size () >= 2 &&
               buffer_[buffer_.size () - 2] == '\x1b') // ST check
      {
        term = true;
      }

      if (term)
      {
        buffer_.clear ();
        state_ = state::normal;
      }
      else
      {
        state_ = state::osc_string;
      }

      // Prevent memory exhaustion on malformed OSC sequences.
      //
      if (buffer_.size () > 1024)
      {
        state_ = state::normal;
        buffer_.clear ();
      }
    }
  };

  // Implementation details.
  //

  template <typename F>
  void terminal_input_parser::
  process_normal (char c, F&& cb)
  {
    // The ground state. We are mostly looking for printable UTF-8 characters
    // here, but we also need to catch the Escape key to switch modes, and a
    // handful of legacy C0 control codes that we care about.
    //

    // Escape (0x1B) is the start of every control sequence.
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

    // C0 Control Codes. These are the single-byte codes that actually mean
    // something for editing.
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
      // Legacy Ctrl+Key mapping. In ASCII, Ctrl+A is 1, Ctrl+B is 2, etc. We
      // map these back to explicit events so the upper layers do not have to
      // deal with raw ASCII values.
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
      // UTF-8 text processing. We accumulate bytes here and only fire an
      // event when we have a semantically complete codepoint. Emitting partial
      // sequences would cause the editor to display replacement characters
      // or corrupt the undo history.
      //
      std::string s (utf8_.process (static_cast<unsigned char> (c)));

      if (!s.empty ())
      {
        // Full grapheme or codepoint assembled.
        //
        cb (text_input_event {std::move (s), key_modifier::none});
      }
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_escape (char b, F&& cb)
  {
    // We just saw an ESC. Determine what mode to enter next.
    //
    switch (b)
    {
      case '[':
        state_ = state::csi; // CSI: ESC [ ...
        buffer_.clear ();
        break;

      case 'O':
        state_ = state::ss3; // SS3: ESC O ...
        break;

      case ']':
        state_ = state::osc; // OSC: ESC ] ...
        buffer_.clear ();
        break;

      default:
        // It is not a standard sequence start. If it is a printable character,
        // this is likely the user pressing Alt+Key, which terminals often send
        // as ESC followed by the character.
        //
        if (std::isprint (static_cast<unsigned char> (b)))
        {
          std::string s (1, b);
          cb (text_input_event {std::move (s), key_modifier::alt});
        }
        else
        {
          // Otherwise, it might be a stray ESC key followed by something else.
          // Emit the ESC as a keypress and re-process the current byte as
          // normal input.
          //
          cb (special_key_event {special_key::escape, key_modifier::none});
          process_normal (b, cb);
        }
        state_ = state::normal;
        break;
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_csi (char c, F&& cb)
  {
    // Check if this is a final byte.
    //
    if (c >= 0x40 && c <= 0x7E)
    {
      // We found the terminator. Dissect the buffer into parameters and
      // intermediates. According to the standard, intermediates always appear
      // after parameters.
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

      // Parse the integer parameters.
      //
      auto v (csi_parser::parse_params (p));

      // Check for private mode prefixes (like '?' in "?1049h"). These sit in
      // the parameter range but completely change the command's meaning.
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
    // Check if this is a valid parameter or intermediate byte.
    //
    else if ((c >= 0x20 && c <= 0x2F) || (c >= 0x30 && c <= 0x3F))
    {
      buffer_.push_back (c);

      // Prevent buffer overflow attacks or stuck states. No CSI sequence
      // should realistically exceed 64 bytes.
      //
      if (buffer_.size () > 64)
        state_ = state::normal;
    }
    else
    {
      // Invalid byte for a CSI sequence. Abort and return to normal to avoid
      // eating the rest of the stream.
      //
      state_ = state::normal;
    }
  }

  template <typename F>
  void terminal_input_parser::
  process_ss3 (char c, F&& cb)
  {
    // Single Shift 3 (SS3) sequences have a strictly fixed length of one
    // character payload. They are mostly used for F1-F4 and cursor keys in
    // Application Mode.
    //
    cb (ss3_parser::interpret_sequence (c));
    state_ = state::normal;
  }
}
