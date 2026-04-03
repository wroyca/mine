#pragma once

#include <vector>
#include <string>
#include <variant>
#include <cstdint>
#include <compare>

#include <mine/mine-types.hxx>
#include <mine/mine-traits.hxx>

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
      // Start of a new sequence?
      //
      if (expect == 0)
      {
        if ((b & 0x80) == 0)
        {
          // ASCII (0xxxxxxx). Single byte.
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
          // Invalid start byte (continuation byte alone or 0xFF).
          // Return Replacement Character U+FFFD.
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
            // Done. Move out the buffer.
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
          // something else. Reset and emit replacement.
          //
          buffer.clear ();
          expect = 0;
          seen = 0;
          return "\xEF\xBF\xBD";
        }
      }

      return {}; // Keep feeding me.
    }

    void
    reset ()
    {
      buffer.clear ();
      expect = 0;
      seen = 0;
    }
  };

  // The Main Parser.
  //
  // Esentially is a state machine that sits between the raw TTY `read()` loop
  // and the application's event loop.
  //
  class terminal_input_parser
  {
  public:
    terminal_input_parser () = default;

    // Pump raw bytes into the parser.
    //
    // `callback` will be invoked 0 or more times with `input_event`. We use a
    // template callback to avoid heap allocation of a `std::vector` return
    // value for every tiny keypress.
    //
    template <typename F>
    void
    parse (const char* data, std::size_t size, F&& cb)
    {
      for (std::size_t i (0); i < size; ++i)
        process_byte (data[i], cb);
    }

    // Is the machine strictly idle?
    //
    // If this returns false after a read timeout, it usually means we have
    // a stranded escape sequence (e.g., the user pressed ESC and waited).
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
      // Format: ESC ] <cmd> ; <payload> (BEL | ST)
      // ST is ESC \.
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

      // Safety: Prevent memory exhaustion on malformed OSC.
      //
      if (buffer_.size () > 1024)
      {
        state_ = state::normal;
        buffer_.clear ();
      }
    }
  };

  // Implementation (Templates)
  //

  template <typename F>
  void terminal_input_parser::
  process_escape (char b, F&& cb)
  {
    // We just saw an ESC. What's next?
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
        // It's not a standard sequence start.
        //
        // If it's a printable character (like 'a'), this is likely the user
        // pressing Alt+A (which terminals often send as ESC followed by 'a').
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
}
