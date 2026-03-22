#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  // Handle the Backspace key.
  //
  // Semantically, backspacing is a compound operation: we attempt to move the
  // cursor "back" (left/up) one grapheme cluster, and if successful, we
  // delete the grapheme at that new position.
  //
  class delete_backward_command: public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      auto b (s.buffer ());
      auto c (s.get_cursor ());

      // See if we have an active selection. If so, backspace simply deletes
      // the marked region.
      //
      if (c.has_mark () && c.mark () != c.position ())
      {
        auto p1 (std::min (c.position (), c.mark ()));
        auto p2 (std::max (c.position (), c.mark ()));

        // Note that terminal cell selections are inclusive. So we must step
        // the end boundary forward by one grapheme to make the exclusive
        // delete_range cover it.
        //
        auto e (cursor (p2).move_right (b).position ());

        auto nb (b.delete_range (p1, e));
        auto nc (c.move_to (p1));

        nc.clear_mark ();

        return s.update (std::move (nb), nc);
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

      return s.update (std::move (nb), nc);
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "delete_backward";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return true;
    }
  };

  // Handle the Delete key.
  //
  // This operation removes the grapheme currently under the cursor (or merges
  // lines if the cursor is at a newline). The cursor position itself remains
  // technically unchanged, though the content shifts "left" to fill the
  // void.
  //
  class delete_forward_command : public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      auto b (s.buffer ());
      auto c (s.get_cursor ());

      // See if we have an active selection. If so, delete simply removes
      // the marked region.
      //
      if (c.has_mark () && c.mark () != c.position ())
      {
        auto p1 (std::min (c.position (), c.mark ()));
        auto p2 (std::max (c.position (), c.mark ()));

        // Note that terminal cell selections are inclusive. So we must step the
        // end boundary forward by one grapheme to make the exclusive
        // delete_range cover it.
        //
        auto e (cursor (p2).move_right (b).position ());

        auto nb (b.delete_range (p1, e));
        auto nc (c.move_to (p1));

        nc.clear_mark ();

        return s.update (std::move (nb), nc);
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

      return s.update (std::move (b1), c);
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "delete_forward";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return true;
    }
  };
}
