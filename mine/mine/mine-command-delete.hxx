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
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // First, try to step back one grapheme.
      //
      auto nc (c.move_left (b));

      // If the cursor position didn't change, we are at the beginning of the
      // buffer and there is nothing to delete.
      //
      if (nc.position () == c.position ())
        return s;

      // Otherwise, delete the grapheme at the new cursor position.
      //
      // Note: delete_next_grapheme() handles line merging if the position
      // points to a newline logic internally.
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
  class delete_forward_command: public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());
      const auto pos (c.position ());

      // Check if we are at the absolute end of the buffer. If so, there is
      // nothing "forward" to delete.
      //
      if (pos.line.value >= b.line_count ())
        return s;

      // Also check if we are at the very end of the last line (past the last
      // character). Unlike other lines where this would trigger a merge with
      // the next line, here it's a no-op.
      //
      if (pos.line.value == b.line_count () - 1 &&
          pos.column.value >= b.line_length (pos.line))
        return s;

      // Delete the grapheme at the current cursor position.
      //
      auto nb (b.delete_next_grapheme (pos));

      return s.update (std::move (nb), c);
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
