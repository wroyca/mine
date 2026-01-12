#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>

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
}
