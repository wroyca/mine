#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  // Backspace logic.
  //
  // The mechanics are: try to move the cursor one step back. If we are
  // blocked (i.e., at the very start of the buffer), we do nothing.
  // Otherwise, we delete the character at that *new* position.
  //
  class delete_backward_command: public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Try to step back.
      //
      auto nc (c.move_left (b));

      if (nc.position () == c.position ())
        return s;

      // Delete the character at the new position. Note that if we moved to
      // the end of the previous line, this deletes the newline, merging
      // the lines.
      //
      auto nb (b.delete_char (nc.position ()));

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

  // Delete key logic.
  //
  // Delete the character *under* the cursor. The cursor itself remains
  // stationary (coordinate-wise), but the content shifts left.
  //
  class delete_forward_command: public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Check bounds. If we are at the EOF, there is nothing to eat.
      //
      // We're at EOF if we're on the last line and at/past its end.
      //
      const auto pos (c.position ());
      if (pos.line.value >= b.line_count ())
        return s;

      if (pos.line.value == b.line_count () - 1 &&
          pos.column.value >= b.line_length (pos.line))
        return s;

      auto nb (b.delete_char (pos));

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
