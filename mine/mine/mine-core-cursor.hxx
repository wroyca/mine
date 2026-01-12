#pragma once

#include <algorithm>

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>
#include <mine/mine-core-buffer.hxx>

namespace mine
{
  // A cursor in grapheme cluster space.
  //
  // This class represents the "point" in the editor. Note that all coordinates
  // here are in terms of *grapheme clusters*, not bytes or code points.
  //
  // If the user presses "Right" while standing on a flag emoji, we increment
  // the column by 1, which might correspond to jumping 10+ bytes in the buffer.
  //
  // In other words, we relies on the buffer to define what "column N" actually
  // means in memory.
  //
  class cursor
  {
  public:
    cursor () = default;

    explicit
    cursor (cursor_position p)
      : p_ (p)
    {
    }

    // Accessors
    //

    cursor_position
    position () const noexcept
    {
      return p_;
    }

    line_number
    line () const noexcept
    {
      return p_.line;
    }

    column_number
    column () const noexcept
    {
      return p_.column;
    }

    // Navigation
    //

    cursor
    move_to (cursor_position p) const noexcept
    {
      return cursor (p);
    }

    // Move backwards (Left Arrow).
    //
    // Standard wrapping logic: if we hit the left wall (column 0), we try to
    // wrap up to the end of the previous line.
    //
    cursor
    move_left (const text_buffer& b) const noexcept
    {
      // If we have room on the current line, just step back.
      //
      if (p_.column.value > 0)
      {
        return cursor (cursor_position (p_.line,
                                        column_number (p_.column.value - 1)));
      }

      // We are at column 0. Try to wrap to the previous line.
      //
      if (p_.line.value > 0)
      {
        line_number l (p_.line.value - 1);

        // Snap to the end of that line.
        //
        std::size_t n (b.line_length (l));

        return cursor (cursor_position (l, column_number (n)));
      }

      // We are at (0,0). Nowhere to go.
      //
      return *this;
    }

    // Move forwards (Right Arrow).
    //
    // Wrapping logic: if we hit the end of the line (past the last grapheme),
    // we wrap to the start of the next line.
    //
    cursor
    move_right (const text_buffer& b) const noexcept
    {
      std::size_t n (b.line_length (p_.line));

      // Check if we are physically within the line boundaries.
      //
      if (p_.column.value < n)
      {
        return cursor (cursor_position (p_.line,
                                        column_number (p_.column.value + 1)));
      }

      // We are at the end. Try to wrap to the start of the next line.
      //
      if (p_.line.value + 1 < b.line_count ())
      {
        return cursor (cursor_position (line_number (p_.line.value + 1),
                                        column_number (0)));
      }

      // End of buffer.
      //
      return *this;
    }

    // Move Up.
    //
    // Vertical movement is slightly tricky because lines have different
    // lengths.
    //
    // The current behavior is a "hard clamp". That is, if we move from a long
    // line to a short line, we clamp the cursor to the end of the short line.
    //
    cursor
    move_up (const text_buffer& b) const noexcept
    {
      if (p_.line.value > 0)
      {
        line_number l (p_.line.value - 1);
        std::size_t n (b.line_length (l));

        // Clamp column to the new line's length.
        //
        column_number c (std::min (p_.column.value, n));

        return cursor (cursor_position (l, c));
      }

      return *this;
    }

    // Move Down.
    //
    // Same clamping logic as move_up.
    //
    cursor
    move_down (const text_buffer& b) const noexcept
    {
      if (p_.line.value + 1 < b.line_count ())
      {
        line_number l (p_.line.value + 1);
        std::size_t n (b.line_length (l));

        column_number c (std::min (p_.column.value, n));

        return cursor (cursor_position (l, c));
      }

      return *this;
    }

    // Semantic Jumps
    //

    cursor
    move_line_start () const noexcept
    {
      return cursor (cursor_position (p_.line, column_number (0)));
    }

    cursor
    move_line_end (const text_buffer& b) const noexcept
    {
      std::size_t n (b.line_length (p_.line));
      return cursor (cursor_position (p_.line, column_number (n)));
    }

    cursor
    move_buffer_start () const noexcept
    {
      return cursor (cursor_position (line_number (0), column_number (0)));
    }

    cursor
    move_buffer_end (const text_buffer& b) const noexcept
    {
      if (b.line_count () == 0)
        return move_buffer_start ();

      line_number l (b.line_count () - 1);
      std::size_t n (b.line_length (l));

      return cursor (cursor_position (l, column_number (n)));
    }

    // Validation.
    //
    // This is useful after an external buffer mutation (like a undo/redo or
    // a background save) where the cursor might end up pointing to a line
    // that no longer exists or a column that is now out of bounds.
    //
    cursor
    clamp_to_buffer (const text_buffer& b) const noexcept
    {
      if (b.line_count () == 0)
        return cursor (cursor_position (line_number (0), column_number (0)));

      // 1. Clamp line index.
      //
      line_number l (std::min (p_.line.value, b.line_count () - 1));

      // 2. Clamp column index.
      //
      std::size_t n (b.line_length (l));
      column_number c (std::min (p_.column.value, n));

      return cursor (cursor_position (l, c));
    }

    auto
    operator<=> (const cursor&) const noexcept = default;

  private:
    cursor_position p_;
  };
}
