#include <mine/mine-cursor.hxx>

#include <algorithm>

using namespace std;

namespace mine
{
  cursor cursor::
  move_to (cursor_position p) const noexcept
  {
    cursor c (*this);
    c.p_ = p;
    return c;
  }

  cursor cursor::
  move_left (const content& b) const noexcept
  {
    // If there's still room on the current line, we just step back by one
    // cluster. Simple enough.
    //
    if (p_.column.value > 0)
      return move_to (cursor_position (p_.line,
                                       column_number (p_.column.value - 1)));

    // We've hit the left wall (column 0). Let's see if we can wrap around to
    // the end of the previous line.
    //
    if (p_.line.value > 0)
    {
      line_number l (p_.line.value - 1);

      // We want to snap exactly to the end of that preceding line.
      //
      size_t n (b.line_length (l));

      return move_to (cursor_position (l, column_number (n)));
    }

    // We are at the very beginning of the buffer (0,0), so there's nowhere left
    // to go. Just stay put.
    //
    return *this;
  }

  cursor cursor::
  move_right (const content& b) const noexcept
  {
    size_t n (b.line_length (p_.line));

    // As long as we haven't crossed the line boundary physically, just advance
    // the column.
    //
    if (p_.column.value < n)
      return move_to (cursor_position (p_.line,
                                       column_number (p_.column.value + 1)));

    // We are at the end of the current line. Attempt to wrap around to the very
    // beginning of the next line.
    //
    if (p_.line.value + 1 < b.line_count ())
      return move_to (cursor_position (line_number (p_.line.value + 1),
                                       column_number (0)));

    return *this;
  }

  cursor cursor::
  move_up (const content& b) const noexcept
  {
    if (p_.line.value > 0)
    {
      line_number l (p_.line.value - 1);
      size_t n (b.line_length (l));

      // Vertical movement can be tricky since lines vary in length. Here we use
      // a hard clamp: if we move from a long line up to a shorter one, we
      // simply clamp the column to the end of the new short line.
      //
      column_number c (min (p_.column.value, n));

      return move_to (cursor_position (l, c));
    }

    return *this;
  }

  cursor cursor::
  move_down (const content& b) const noexcept
  {
    if (p_.line.value + 1 < b.line_count ())
    {
      line_number l (p_.line.value + 1);
      size_t n (b.line_length (l));

      // Similar to moving up, we clamp the column if the destination line
      // happens to be shorter than our current column position.
      //
      column_number c (min (p_.column.value, n));

      return move_to (cursor_position (l, c));
    }

    return *this;
  }

  cursor cursor::
  move_line_start () const noexcept
  {
    // Jump straight to the first column.
    //
    return move_to (cursor_position (p_.line, column_number (0)));
  }

  cursor cursor::
  move_line_end (const content& b) const noexcept
  {
    // Find how many clusters are in this line and snap to the end.
    //
    size_t n (b.line_length (p_.line));
    return move_to (cursor_position (p_.line, column_number (n)));
  }

  cursor cursor::
  move_buffer_start () const noexcept
  {
    return move_to (cursor_position (line_number (0), column_number (0)));
  }

  cursor cursor::
  move_buffer_end (const content& b) const noexcept
  {
    // An empty buffer is a special case. If there are no lines, we just point
    // to (0,0).
    //
    if (b.line_count () == 0)
      return move_buffer_start ();

    line_number l (b.line_count () - 1);
    size_t n (b.line_length (l));

    return move_to (cursor_position (l, column_number (n)));
  }

  cursor cursor::
  clamp_to_buffer (const content& b) const noexcept
  {
    cursor c (*this);

    // First, handle the pathological case where the buffer has been completely
    // cleared. We must reset to (0,0) and drag the mark along if it exists.
    //
    if (b.line_count () == 0)
    {
      c.p_ = cursor_position (line_number (0), column_number (0));

      if (c.m_)
        c.m_ = c.p_;

      return c;
    }

    // We might be totally out of bounds if lines were deleted by an external
    // mutation (like undo/redo or a background save), so we first clamp the
    // line index.
    //
    line_number l (min (p_.line.value, b.line_count () - 1));

    // Now that we have a valid line, we clamp the column so that it doesn't
    // exceed the line's new length.
    //
    size_t n (b.line_length (l));
    column_number cn (min (p_.column.value, n));

    c.p_ = cursor_position (l, cn);

    // Don't forget the selection mark. If the buffer changed under us, the mark
    // might also be pointing into the void, so we repeat the clamping process
    // for it.
    //
    if (c.m_)
    {
      line_number ml (min (c.m_->line.value, b.line_count () - 1));
      size_t mn (b.line_length (ml));
      column_number mc (min (c.m_->column.value, mn));

      c.m_ = cursor_position (ml, mc);
    }

    return c;
  }
}
