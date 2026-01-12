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
  // Key modifier bitmask.
  //
  // We use a strong enum with bitwise operators to keep this type-safe
  // while allowing combination (e.g., Ctrl | Alt).
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

  // Non-character keys.
  //
  // Note that 'Enter', 'Tab', and 'Backspace' are included here even though
  // they often produce ASCII codes (CR/LF, \t, \b/DEL), because treating
  // them semantically often leads to cleaner editor logic.
  //
  enum class special_key
  {
    // Navigation.
    //
    up, down, left, right,
    home, end,
    page_up, page_down,

    // Editing.
    //
    backspace, delete_key,
    insert,
    tab,
    enter,
    escape,

    // Function keys.
    //
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,

    // Fallback.
    //
    unknown
  };

  // Input Events.
  //

  // A standard printable character (utf-8 byte or ascii).
  //
  struct key_press_event
  {
    char ch;
    key_modifier modifiers;

    bool operator== (const key_press_event&) const = default;
  };

  // A non-printable control key.
  //
  struct special_key_event
  {
    special_key key;
    key_modifier modifiers;

    bool operator== (const special_key_event&) const = default;
  };

  // XTerm SGR mouse reporting event.
  //
  struct mouse_event
  {
    std::uint16_t x;
    std::uint16_t y;
    std::uint8_t button;
    key_modifier modifiers;

    bool operator== (const mouse_event&) const = default;
  };

  // Terminal window resize.
  //
  struct resize_event
  {
    screen_size new_size;

    bool operator== (const resize_event&) const = default;
  };

  // The sum type of all possible terminal inputs.
  //
  using input_event = std::variant<key_press_event,
                                   special_key_event,
                                   mouse_event,
                                   resize_event>;

  // Parser.
  //
  // Basically a state machine compatible with VT100/VT220/XTerm
  // escape sequences. That is, we maintains state across `parse()` calls to
  // handle fragmented inputs (e.g., packet splitting over the network).
  //
  // The general flow is:
  //   Normal -> Escape (seen ESC) -> CSI (seen [) -> Parameters -> Final
  //                               -> SS3 (seen O) -> Final
  //
  class terminal_input_parser
  {
  public:
    terminal_input_parser () = default;

    // Callback-based parse to avoid allocating a return vector.
    //
    // Consumes raw bytes from the TTY and invokes the callback for each
    // parsed event. The parser maintains state across calls to handle
    // fragmented inputs (e.g., packet splitting).
    //
    template <typename F>
    void
    parse (const char* data, std::size_t size, F&& callback)
    {
      for (std::size_t i (0); i < size; ++i)
        process_byte (data[i], callback);
    }

    // Is the state machine resting?
    //
    // Useful for checking if we have a lingering partial escape sequence
    // (which might indicate a timeout or a malformed stream).
    //
    bool
    is_clean () const noexcept
    {
      return state_ == state::normal && buffer_.empty ();
    }

    // Force a reset to normal state.
    //
    void
    reset () noexcept
    {
      state_ = state::normal;
      buffer_.clear ();
    }

  private:
    enum class state
    {
      normal,      // Normal text flow.
      escape,      // Saw '\e'.
      csi,         // Saw '\e[' (Control Sequence Introducer).
      ss3,         // Saw '\eO' (Single Shift 3).
      osc,         // Saw '\e]' (Operating System Command).
      osc_string   // Reading the content of an OSC.
    };

    state state_ {state::normal};

    // Buffer for parameters and intermediates during a sequence parse.
    //
    // 64 bytes is generally enough for even the most complex SGR mouse
    // sequences. If we exceed this, we likely have garbage input.
    //
    std::string buffer_;

    // Template callback-based dispatchers.
    //
    template <typename F>
    void
    process_byte (char b, F&& cb)
    {
      switch (state_)
      {
        case state::normal:
          process_normal (b, cb);
          break;

        case state::escape:
          process_escape (b, cb);
          break;

        case state::csi:
          process_csi (b, cb);
          break;

        case state::ss3:
          process_ss3 (b, cb);
          break;

        case state::osc:
        case state::osc_string:
          process_osc (b, cb);
          break;
      }
    }

    template <typename F>
    void
    process_normal (char b, F&& callback);

    template <typename F>
    void
    process_escape (char b, F&& callback);

    template <typename F>
    void
    process_csi (char b, F&& callback);

    template <typename F>
    void
    process_ss3 (char b, F&& callback);

    template <typename F>
    void
    process_osc (char b, F&& /*cb*/)
    {
      // OSC handling: just consume and ignore for now.
      //
      // Format: ESC ] <cmd> ; <string> (BEL | ST)
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
               buffer_[buffer_.size () - 2] == '\x1b') // ESC \ (ST)
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

      // Safety limit.
      //
      if (buffer_.size () > 1024)
      {
        state_ = state::normal;
        buffer_.clear ();
      }
    }
  };

  // Template implementations that need access to helper classes.
  // Must be defined after the class declaration.
  //
  template <typename F>
  void terminal_input_parser::
  process_escape (char b, F&& cb)
{
    switch (b)
    {
    case '[':
      state_ = state::csi;
      buffer_.clear ();
      break;

    case 'O':
      state_ = state::ss3;
      break;

    case ']':
      state_ = state::osc;
      buffer_.clear ();
      break;

    default:
      // Two-byte sequence (ESC X).
      //
      // If X is printable, it's usually Alt+X. Otherwise, we treat it as
      // a stray ESC followed by a normal byte.
      //
      if (std::isprint (static_cast<unsigned char> (b)))
      {
        cb (key_press_event {b, key_modifier::alt});
      }
      else
      {
        cb (special_key_event {special_key::escape, key_modifier::none});
        process_normal (b, cb);
      }
      state_ = state::normal;
      break;
    }
  }
}
