#pragma once

#include <algorithm>

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>
#include <mine/mine-core-buffer.hxx>

namespace mine
{
  // A point in the 2D text grid.
  //
  // We keep this purely geometric: it knows "where" it is, but it doesn't
  // know "what" is there until we ask it to move relative to a buffer.
  //
  // Note on "Phantom Columns":
  // One tricky aspect of text editing is vertical movement. If you are on
  // column 100 and move down to a line with 5 characters, you snap to 5.
  // If you move down again to a long line, ideally you should snap *back*
  // to 100.
  //
  // This class does *not* track that "phantom" 100. It implements the
  // strict physical clamping. The state management layer (editor_state)
  // is responsible for tracking the desired column if we want that behavior.
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

    // Accessors.
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

    // Navigation.
    //
    // These functions return a *new* cursor. The philosophy here is permissive:
    // if a move is invalid (e.g., move_up() at the top), we return *this*
    // unchanged rather than throwing or returning null. This simplifies
    // the command layer.
    //

    cursor
    move_to (cursor_position p) const noexcept
    {
      return cursor (p);
    }

    // Step backward.
    //
    // If we are at the start of a line (column 0), we wrap to the end of
    // the previous line. If we are at the very start of the buffer (0,0),
    // we stay put.
    //
    cursor
    move_left (const text_buffer& b) const noexcept
    {
      // Simple case: middle of a line.
      //
      if (p_.column.value > 0)
      {
        return cursor (cursor_position (p_.line,
                                        column_number (p_.column.value - 1)));
      }
      // Wrap to previous line?
      //
      else if (p_.line.value > 0)
      {
        line_number l (p_.line.value - 1);
        std::size_t n (b.line_length (l));

        return cursor (cursor_position (l, column_number (n)));
      }

      // Hard stop at (0,0).
      //
      return *this;
    }

    // Step forward.
    //
    // Similarly, we wrap to the start of the next line if we hit the end.
    // Note that "end of line" here includes the position *after* the last
    // character (where the newline effectively lives).
    //
    cursor
    move_right (const text_buffer& b) const noexcept
    {
      std::size_t n (b.line_length (p_.line));

      // Can we step forward on this line?
      //
      // Note: valid positions are 0..n inclusive.
      //
      if (p_.column.value < n)
      {
        return cursor (cursor_position (p_.line,
                                        column_number (p_.column.value + 1)));
      }
      // Wrap to next line?
      //
      else if (p_.line.value + 1 < b.line_count ())
      {
        return cursor (cursor_position (line_number (p_.line.value + 1),
                                        column_number (0)));
      }

      // EOF.
      //
      return *this;
    }

    // Vertical movement.
    //
    // As mentioned above, this implements "clamping". If the target line
    // is shorter than our current column, we snap to the end of that line.
    //
    cursor
    move_up (const text_buffer& b) const noexcept
    {
      if (p_.line.value > 0)
      {
        line_number l (p_.line.value - 1);
        std::size_t n (b.line_length (l));

        // Clamp 'c' to the length of the new line.
        //
        column_number c (std::min (p_.column.value, n));

        return cursor (cursor_position (l, c));
      }

      return *this;
    }

    cursor
    move_down (const text_buffer& b) const noexcept
    {
      if (p_.line.value + 1 < b.line_count ())
      {
        line_number l (p_.line.value + 1);
        std::size_t n (b.line_length (l));

        // Clamp.
        //
        column_number c (std::min (p_.column.value, n));

        return cursor (cursor_position (l, c));
      }

      return *this;
    }

    // Semantic Jumps (Home/End).
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

    // Sanitize the cursor.
    //
    // When the buffer changes "under our feet" (e.g., Undo, or a background
    // reload), then it might be pointing to line 500 whereas buffer now only
    // has 10 lines. This pulls it back into valid territory.
    //
    cursor
    clamp_to_buffer (const text_buffer& b) const noexcept
    {
      if (b.line_count () == 0)
        return cursor (cursor_position (line_number (0), column_number (0)));

      // Clamp line index.
      //
      line_number l (std::min (p_.line.value, b.line_count () - 1));

      // Clamp column index (based on the *clamped* line's length).
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
