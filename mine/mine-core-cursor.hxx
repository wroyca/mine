#pragma once

#include <algorithm>

#include <mine/mine-types.hxx>
#include <mine/mine-contract.hxx>
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

    // Selection mark.
    //

    // Set the selection mark at the current point.
    //
    // This anchors one end of the selection. Note that as the point moves
    // subsequently, the region between the mark and the point forms the
    // active selection highlight.
    //
    void
    set_mark () noexcept
    {
      m_ = p_;
    }

    // Clear the active selection.
    //
    // We typically trigger this when the user presses Escape or performs
    // an action that cancels the highlight.
    //
    void
    clear_mark () noexcept
    {
      m_.reset ();
    }

    // Return true if we have an active selection.
    //
    bool
    has_mark () const noexcept
    {
      return m_.has_value ();
    }

    // Get the current mark position.
    //
    cursor_position
    mark () const noexcept
    {
      MINE_PRECONDITION (has_mark ());

      return *m_;
    }

    // Navigation
    //

    cursor
    move_to (cursor_position p) const noexcept
    {
      cursor c (*this);
      c.p_ = p;
      return c;
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
        return move_to (cursor_position (p_.line,
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

        return move_to (cursor_position (l, column_number (n)));
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
        return move_to (cursor_position (p_.line,
                                         column_number (p_.column.value + 1)));
      }

      // We are at the end. Try to wrap to the start of the next line.
      //
      if (p_.line.value + 1 < b.line_count ())
      {
        return move_to (cursor_position (line_number (p_.line.value + 1),
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

        return move_to (cursor_position (l, c));
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

        return move_to (cursor_position (l, c));
      }

      return *this;
    }

    // Semantic Jumps
    //

    cursor
    move_line_start () const noexcept
    {
      return move_to (cursor_position (p_.line, column_number (0)));
    }

    cursor
    move_line_end (const text_buffer& b) const noexcept
    {
      std::size_t n (b.line_length (p_.line));
      return move_to (cursor_position (p_.line, column_number (n)));
    }

    cursor
    move_buffer_start () const noexcept
    {
      return move_to (cursor_position (line_number (0), column_number (0)));
    }

    cursor
    move_buffer_end (const text_buffer& b) const noexcept
    {
      if (b.line_count () == 0)
        return move_buffer_start ();

      line_number l (b.line_count () - 1);
      std::size_t n (b.line_length (l));

      return move_to (cursor_position (l, column_number (n)));
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
      cursor c (*this);

      if (b.line_count () == 0)
      {
        c.p_ = cursor_position (line_number (0), column_number (0));
        if (c.m_)
          c.m_ = c.p_;
        return c;
      }

      // 1. Clamp line index.
      //
      line_number l (std::min (p_.line.value, b.line_count () - 1));

      // 2. Clamp column index.
      //
      std::size_t n (b.line_length (l));
      column_number cn (std::min (p_.column.value, n));

      c.p_ = cursor_position (l, cn);

      if (c.m_)
      {
        line_number ml (std::min (c.m_->line.value, b.line_count () - 1));
        std::size_t mn (b.line_length (ml));
        column_number mc (std::min (c.m_->column.value, mn));
        c.m_ = cursor_position (ml, mc);
      }

      return c;
    }

    auto
    operator<=> (const cursor&) const noexcept = default;

  private:
    cursor_position p_;

    // The selection anchor. If this has a value, a selection is active.
    //
    std::optional<cursor_position> m_;
  };
}
