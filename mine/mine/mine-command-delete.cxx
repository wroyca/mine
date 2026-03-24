#include <mine/mine-command-delete.hxx>

using namespace std;

namespace mine
{
  editor_state delete_backward_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, backspace simply deletes the
    // marked region.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Note that terminal cell selections are inclusive. So we must step the
      // end boundary forward by one grapheme to make the exclusive delete_range
      // cover it.
      //
      auto e (cursor (p2).move_right (b).position ());

      auto nb (b.delete_range (p1, e));
      auto nc (c.move_to (p1));

      nc.clear_mark ();

      return s.update (move (nb), nc);
    }

    // Clear the empty mark so it doesn't linger and cause trouble later.
    //
    c.clear_mark ();

    // First, try to step back by one grapheme.
    //
    auto nc (c.move_left (b));

    // If the cursor position didn't change, we must be at the very beginning
    // of the buffer. In this case, there is naturally nothing left to delete.
    //
    if (nc.position () == c.position ())
      return s.with_cursor (c);

    // Otherwise, delete the grapheme at the newly computed position.
    //
    // Note that we rely on delete_next_grapheme() to internally take care of
    // the line merging logic if this position happens to point to a newline.
    //
    auto nb (b.delete_next_grapheme (nc.position ()));

    return s.update (move (nb), nc);
  }

  string_view delete_backward_command::
  name () const noexcept
  {
    return "delete_backward";
  }

  bool delete_backward_command::
  modifies_buffer () const noexcept
  {
    return true;
  }

  editor_state delete_forward_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, delete simply removes
    // the marked region.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

      // Note that terminal cell selections are inclusive. So we must step the
      // end boundary forward by one grapheme to make the exclusive
      // delete_range cover it.
      //
      auto e (cursor (p2).move_right (b).position ());

      auto nb (b.delete_range (p1, e));
      auto nc (c.move_to (p1));

      nc.clear_mark ();

      return s.update (move (nb), nc);
    }

    c.clear_mark (); // Clear empty mark, if any.
    const auto p (c.position ());

    // See if we are already at the absolute end of the buffer. If so, there is
    // naturally nothing further to delete.
    //
    if (p.line.value >= b.line_count ())
      return s.with_cursor (c);

    // We must also handle the boundary case where we sit exactly past the
    // last character of the final line. For any normal line this situation
    // would trigger a merge with the line below, but for the final line
    // it amounts to a no-op.
    //
    if (p.line.value == b.line_count () - 1 &&
        p.column.value >= b.line_length (p.line))
      return s.with_cursor (c);

    // At this point we can safely delete the next grapheme starting from the
    // current position.
    //
    auto b1 (b.delete_next_grapheme (p));

    return s.update (move (b1), c);
  }

  string_view delete_forward_command::
  name () const noexcept
  {
    return "delete_forward";
  }

  bool delete_forward_command::
  modifies_buffer () const noexcept
  {
    return true;
  }
}
