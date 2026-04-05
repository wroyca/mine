#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>
#include <functional>

namespace mine
{
  // Editor Primitives
  //

  // 0-based line index.
  //
  // Internally we are always 0-based because that aligns with C++ vectors and
  // arrays. We only convert to 1-based when rendering the UI (status bar,
  // line numbers column) because humans prefer counting from 1.
  //
  struct line_number
  {
    std::size_t value;

    constexpr explicit
    line_number (std::size_t v = 0) noexcept
      : value (v)
    {
    }

    constexpr auto
    operator<=> (const line_number&) const = default;

    constexpr line_number&
    operator++ () noexcept
    {
      ++value;
      return *this;
    }

    constexpr line_number
    operator++ (int) noexcept
    {
      line_number r (*this);
      ++value;
      return r;
    }
  };

  // 0-based grapheme cluster index within a line.
  //
  // Note that this represents the position in terms of user-visible characters
  // (graphemes), not bytes and not code points. If the user hits the "Right
  // Arrow" key, this value increments by exactly one, regardless of whether
  // they stepped over an 'a' (1 byte) or a '🙂' (4 bytes).
  //
  struct column_number
  {
    std::size_t value;

    constexpr explicit
    column_number (std::size_t v = 0) noexcept
      : value (v)
    {
    }

    constexpr auto
    operator<=> (const column_number&) const = default;

    constexpr column_number&
    operator++ () noexcept
    {
      ++value;
      return *this;
    }

    constexpr column_number
    operator++ (int) noexcept
    {
      column_number r (*this);
      ++value;
      return r;
    }
  };

  // A logical location in the text buffer.
  //
  // This is the "Model" coordinate. It refers to infinite space.
  //
  struct cursor_position
  {
    line_number   line;
    column_number column;

    constexpr
    cursor_position (line_number l = line_number (0),
                     column_number c = column_number (0)) noexcept
      : line (l), column (c)
    {
    }

    constexpr auto
    operator<=> (const cursor_position&) const = default;
  };

  // Screen Primitives
  //

  // A physical location on the terminal grid.
  //
  // This is the "View" coordinate. Unlike buffer coordinates which can grow
  // indefinitely (`size_t`), screen coordinates are physically constrained by
  // the terminal protocol and window size.
  //
  // VT100/ANSI usually limits these to unsigned short range, so `uint16_t` is
  // strictly correct and saves us significant memory in the render grid,
  // which we allocate primarily as a large vector of cells.
  //
  struct screen_position
  {
    std::uint16_t row;
    std::uint16_t col;

    constexpr
    screen_position (std::uint16_t r = 0, std::uint16_t c = 0) noexcept
      : row (r), col (c)
    {
    }

    constexpr auto
    operator<=> (const screen_position&) const = default;
  };

  struct screen_size
  {
    std::uint16_t rows;
    std::uint16_t cols;

    constexpr
    screen_size (std::uint16_t r = 0, std::uint16_t c = 0) noexcept
      : rows (r), cols (c)
    {
    }

    constexpr auto
    operator<=> (const screen_size&) const = default;

    constexpr bool
    contains (screen_position p) const noexcept
    {
      // Simple bounds check. Since we use unsigned types, we don't need to
      // check for < 0.
      //
      return p.row < rows && p.col < cols;
    }
  };

  // Low-level Types
  //

  // Absolute byte offset in a file/buffer.
  //
  // We rarely use this in high-level editor logic (which prefers line/col),
  // but it's necessary for file I/O and interaction with C-style APIs.
  //
  struct byte_offset
  {
    std::size_t value;

    constexpr explicit
    byte_offset (std::size_t v = 0) noexcept
      : value (v)
    {
    }

    constexpr auto
    operator<=> (const byte_offset&) const = default;
  };

  // A single Unicode code point.
  //
  // This is the 32-bit integer representation of a character (U+XXXX).
  //
  struct codepoint
  {
    std::uint32_t value;

    constexpr explicit
    codepoint (std::uint32_t v = 0) noexcept
      : value (v)
    {
    }

    constexpr auto
    operator<=> (const codepoint&) const = default;

    // Check if this is a valid Unicode scalar value.
    //
    // The Unicode standard defines valid scalars as:
    //
    // 1. 0x0000 to 0xD7FF (Basic Multilingual Plane, excluding surrogates)
    // 2. 0xE000 to 0x10FFFF (The rest of the space)
    //
    // Surrogates (0xD800-0xDFFF) are reserved for UTF-16 encoding mechanisms
    // and are invalid as standalone scalar values.
    //
    constexpr bool
    is_valid () const noexcept
    {
      return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
    }
  };

  // High-resolution timestamp.
  //
  // Used for measuring input latency and render times. We wrap it to avoid
  // confusion with standard library time points or raw integers.
  //
  struct timestamp
  {
    std::uint64_t nanoseconds;

    constexpr explicit
    timestamp (std::uint64_t ns = 0) noexcept
      : nanoseconds (ns)
    {
    }

    constexpr auto
    operator<=> (const timestamp&) const = default;

    constexpr timestamp
    operator- (timestamp t) const noexcept
    {
      return timestamp (nanoseconds - t.nanoseconds);
    }
  };

  // Pixel measurements.
  //
  using pixel_t       = float;
  using pixel_offset  = std::int32_t;

  // Editor identity types.
  //

  struct buffer_id
  {
    std::uint32_t value;

    constexpr explicit
    buffer_id (std::uint32_t v = 0) noexcept
      : value (v)
    {
    }

    constexpr auto
    operator<=> (const buffer_id&) const = default;

    constexpr buffer_id&
    operator++ () noexcept
    {
      ++value;
      return *this;
    }

    constexpr buffer_id
    operator++ (int) noexcept
    {
      buffer_id r (*this);
      ++value;
      return r;
    }
  };

  struct window_id
  {
    std::uint32_t value;

    constexpr explicit
    window_id (std::uint32_t v = 0) noexcept
      : value (v)
    {
    }

    constexpr auto
    operator<=> (const window_id&) const = default;

    constexpr window_id&
    operator++ () noexcept
    {
      ++value;
      return *this;
    }

    constexpr window_id
    operator++ (int) noexcept
    {
      window_id r (*this);
      ++value;
      return r;
    }
  };

  // Handle types (opaque identifiers).
  //
  struct handle
  {
    std::uint32_t id = 0;

    bool valid () const { return id != 0; }
    explicit operator bool () const { return valid (); }

    bool operator== (const handle&) const = default;
    bool operator!= (const handle&) const = default;
  };

  // Specialized handles.
  //
  struct texture_handle : handle {};
  struct shader_handle  : handle {};
  struct buffer_handle  : handle {};
  struct atlas_handle   : handle {};

  // Color (RGBA, normalized floats).
  //
  struct color
  {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    static color white ()      { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static color black ()      { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static color transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    bool operator== (const color&) const = default;
  };

  // Rectangle (screen space).
  //
  struct rect
  {
    pixel_t x      = 0.0f;
    pixel_t y      = 0.0f;
    pixel_t width  = 0.0f;
    pixel_t height = 0.0f;

    [[nodiscard]] pixel_t right () const  { return x + width; }
    [[nodiscard]] pixel_t bottom () const { return y + height; }

    [[nodiscard]] bool contains (pixel_t px, pixel_t py) const
    {
      return px >= x && px < right () && py >= y && py < bottom ();
    }

    bool operator== (const rect&) const = default;
  };

  // UV coordinates for texture mapping.
  //
  struct uv_rect
  {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;

    bool operator== (const uv_rect&) const = default;
  };
}

// Hash specializations for strong ID types.
//
template <>
struct std::hash<mine::buffer_id>
{
  constexpr std::size_t
  operator() (mine::buffer_id id) const noexcept
  {
    return std::hash<std::uint32_t> {} (id.value);
  }
};

template <>
struct std::hash<mine::window_id>
{
  constexpr std::size_t
  operator() (mine::window_id id) const noexcept
  {
    return std::hash<std::uint32_t> {} (id.value);
  }
};
