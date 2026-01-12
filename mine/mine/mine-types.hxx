#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>
#include <string_view>

namespace mine
{
  // Editor Primitives.
  //
  // We use strong types here effectively to prevent the classic "swapped
  // arguments" bugs. It is far too easy to pass (col, row) to a function
  // expecting (row, col) when they are both just `size_t`.
  //

  // 0-based line index.
  //
  // Internally we are always 0-based. We only convert to 1-based when
  // rendering the UI (status bar, line numbers column).
  //
  struct line_number
  {
    std::size_t value;

    constexpr explicit line_number (std::size_t v = 0) noexcept
      : value (v) {}

    constexpr auto operator<=> (const line_number&) const = default;

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

  // 0-based column index (byte offset).
  //
  // Note that this is a *byte* offset, not a grapheme cluster offset.
  // When dealing with UTF-8, arithmetic on this type requires care.
  //
  struct column_number
  {
    std::size_t value;

    constexpr explicit column_number (std::size_t v = 0) noexcept
      : value (v) {}

    constexpr auto operator<=> (const column_number&) const = default;

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

  // A location in the text buffer.
  //
  struct cursor_position
  {
    line_number line;
    column_number column;

    constexpr cursor_position (line_number l = line_number (0),
                               column_number c = column_number (0)) noexcept
      : line (l), column (c) {}

    constexpr auto operator<=> (const cursor_position&) const = default;
  };

  // Screen Primitives.
  //
  // Unlike buffer coordinates which can grow indefinitely (`size_t`),
  // screen coordinates are physically constrained by the terminal protocol.
  // VT100/ANSI usually limits these to unsigned short range, so `uint16_t`
  // saves us some memory in the render grid.
  //

  struct screen_position
  {
    std::uint16_t row;
    std::uint16_t col;

    constexpr screen_position (std::uint16_t r = 0, std::uint16_t c = 0) noexcept
      : row (r), col (c) {}

    constexpr auto operator<=> (const screen_position&) const = default;
  };

  struct screen_size
  {
    std::uint16_t rows;
    std::uint16_t cols;

    constexpr screen_size (std::uint16_t r = 0, std::uint16_t c = 0) noexcept
      : rows (r), cols (c) {}

    constexpr auto operator<=> (const screen_size&) const = default;

    constexpr bool
    contains (screen_position pos) const noexcept
    {
      return pos.row < rows && pos.col < cols;
    }
  };

  // Low-level types.
  //

  // Absolute offset in a file/buffer.
  //
  struct byte_offset
  {
    std::size_t value;

    constexpr explicit byte_offset (std::size_t v = 0) noexcept
      : value (v) {}

    constexpr auto operator<=> (const byte_offset&) const = default;
  };

  // Unicode code point.
  //
  struct codepoint
  {
    std::uint32_t value;

    constexpr explicit codepoint (std::uint32_t v = 0) noexcept
      : value (v) {}

    constexpr auto operator<=> (const codepoint&) const = default;

    // Check if this is a valid Unicode scalar value.
    //
    // Ranges: [0, 0xD7FF] and [0xE000, 0x10FFFF].
    // Excludes surrogates (0xD800-0xDFFF) and anything above 0x10FFFF.
    //
    constexpr bool
    is_valid () const noexcept
    {
      return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
    }
  };

  // High-resolution timestamp for latency metrics.
  //
  struct timestamp
  {
    std::uint64_t nanoseconds;

    constexpr explicit timestamp (std::uint64_t ns = 0) noexcept
      : nanoseconds (ns) {}

    constexpr auto operator<=> (const timestamp&) const = default;

    constexpr timestamp
    operator- (timestamp other) const noexcept
    {
      return timestamp (nanoseconds - other.nanoseconds);
    }
  };
}
