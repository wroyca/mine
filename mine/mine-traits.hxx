#pragma once

#include <cstddef>
#include <string_view>
#include <type_traits>

#include <mine/mine-types.hxx>

namespace mine
{
  // Platform capabilities.
  //
  // While the build system usually handles compile-time configuration,
  // having these as constexpr booleans allows us to use `if constexpr`
  // inside function bodies.
  //
  struct platform_traits
  {
#if defined(__linux__) || defined(__gnu_linux__)
    static constexpr bool is_linux   = true;
    static constexpr bool is_posix   = true;
    static constexpr bool is_windows = false;
#elif defined(__APPLE__) && defined(__MACH__)
    static constexpr bool is_linux   = false;
    static constexpr bool is_posix   = true;
    static constexpr bool is_windows = false;
#elif defined(_WIN32) || defined(_WIN64)
    static constexpr bool is_linux   = false;
    static constexpr bool is_posix   = false;
    static constexpr bool is_windows = true;
#else
    static constexpr bool is_linux   = false;
    static constexpr bool is_posix   = false;
    static constexpr bool is_windows = false;
#endif

    // Capabilities derived from the OS type.
    //
    static constexpr bool has_raw_terminal    = is_posix || is_windows;
    static constexpr bool has_signal_handling = is_posix;
  };

  // UTF-8 Helpers.
  //
  // We strictly assume standard UTF-8 (RFC 3629). We need to interpret
  // bytes to handle cursor movement correctly (moving over a multi-byte
  // character requires jumping multiple bytes in the buffer).
  //
  struct utf8_traits
  {
    // Bitmasks for classification:
    // 0xxxxxxx (0x00-0x7F): ASCII
    // 10xxxxxx (0x80-0xBF): Continuation
    // 110xxxxx (0xC0-0xDF): 2-byte start
    // 1110xxxx (0xE0-0xEF): 3-byte start
    // 11110xxx (0xF0-0xF7): 4-byte start
    //

    static constexpr bool
    is_single_byte (unsigned char b) noexcept
    {
      return (b & 0x80) == 0;
    }

    static constexpr bool
    is_continuation (unsigned char b) noexcept
    {
      return (b & 0xC0) == 0x80;
    }

    static constexpr bool
    is_lead_byte (unsigned char b) noexcept
    {
      // It's a lead byte if it has the high bit set but isn't a continuation.
      //
      return (b & 0xC0) == 0xC0 && !is_continuation (b);
    }

    // Determine the total length of the sequence based on the lead byte.
    // Returns 0 if the byte is invalid or a continuation byte.
    //
    static constexpr std::size_t
    sequence_length (unsigned char lead) noexcept
    {
      if ((lead & 0x80) == 0)    return 1;
      if ((lead & 0xE0) == 0xC0) return 2;
      if ((lead & 0xF0) == 0xE0) return 3;
      if ((lead & 0xF8) == 0xF0) return 4;
      return 0;
    }

    // A lightweight validator.
    //
    // Note that this doesn't check for overlong encodings or valid codepoint
    // ranges (surrogates, etc). It just checks that the byte structure matches
    // the length advertised by the lead byte.
    //
    static constexpr bool
    is_valid_sequence (std::string_view sv) noexcept
    {
      if (sv.empty ())
        return false;

      std::size_t len (sequence_length (static_cast<unsigned char> (sv[0])));

      if (len == 0 || len > sv.size ())
        return false;

      for (std::size_t i (1); i < len; ++i)
      {
        if (!is_continuation (static_cast<unsigned char> (sv[i])))
          return false;
      }

      return true;
    }
  };

  // Terminal Control Sequences (ECMA-48).
  //
  // Used by the input parser to categorize incoming bytes.
  //
  struct terminal_traits
  {
    enum class sequence_type
    {
      none,
      esc,   // ESC (0x1B)
      csi,   // ESC [
      ss3,   // ESC O
      osc    // ESC ]
    };

    static constexpr bool
    is_escape (char c) noexcept
    {
      return c == '\x1b';
    }

    // Sequence introducers (following ESC).
    //
    static constexpr bool
    is_csi_start (char c) noexcept { return c == '['; }

    static constexpr bool
    is_ss3_start (char c) noexcept { return c == 'O'; }

    static constexpr bool
    is_osc_start (char c) noexcept { return c == ']'; }

    // Parameter bytes: 0x30-0x3F ('0'-'9', ':', ';', '<', '=', '>', '?').
    //
    static constexpr bool
    is_csi_parameter (char c) noexcept
    {
      return c >= 0x30 && c <= 0x3F;
    }

    // Intermediate bytes: 0x20-0x2F (Space, '!', '"', ..., '/').
    //
    static constexpr bool
    is_csi_intermediate (char c) noexcept
    {
      return c >= 0x20 && c <= 0x2F;
    }

    // Final bytes: 0x40-0x7E ('@' - '~').
    //
    // This marks the end of a CSI sequence and determines the command (e.g.,
    // 'A' for Up, 'm' for SGR).
    //
    static constexpr bool
    is_csi_final (char c) noexcept
    {
      return c >= 0x40 && c <= 0x7E;
    }
  };

  // Rendering Backends.
  //
  // We use traits to configure the renderer at compile time. This allows us
  // to swap between a TTY backend (ANSI codes) and potentially a GUI backend
  // (OpenGL/Vulkan) in the future without runtime overhead.
  //

  template <typename B>
  struct rendering_traits;

  struct terminal_backend;
  struct opengl_backend;

  template <>
  struct rendering_traits<terminal_backend>
  {
    static constexpr bool supports_color          = true;
    static constexpr bool supports_unicode        = true;
    static constexpr bool requires_redraw_diff    = true;  // Bandwidth constrained.
    static constexpr bool is_hardware_accelerated = false;
  };

  template <>
  struct rendering_traits<opengl_backend>
  {
    static constexpr bool supports_color          = true;
    static constexpr bool supports_unicode        = true;
    static constexpr bool requires_redraw_diff    = false; // Just redraw the quad.
    static constexpr bool is_hardware_accelerated = true;
  };
}
