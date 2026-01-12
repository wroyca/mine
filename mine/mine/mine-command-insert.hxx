#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  // Normal typing.
  //
  // We insert the character and rely on the cursor's move logic to handle
  // advancement. If we implement wrapping or tab expansion later, the command
  // doesn't need to know about it; it just says "step right".
  //
  class insert_char_command: public command
  {
  public:
    explicit
    insert_char_command (char c)
        : c_ (c)
    {
    }

    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Mutate the buffer.
      //
      auto nb (b.insert_char (c.position (), c_));

      // Calculate new cursor position based on the new content.
      //
      auto nc (c.move_right (nb));

      return s.update (std::move (nb), nc);
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "insert_char";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return true;
    }

  private:
    char c_;
  };

  // Hard return (Enter).
  //
  // Splits the current line and forces the cursor to the beginning of the
  // next line (implicit carriage return).
  //
  class insert_newline_command: public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      auto nb (b.insert_newline (c.position ()));

      // Reset to column 0 on the new line.
      //
      // Note: We access .value directly here assuming strong types for
      // line/col numbers.
      //
      cursor nc (cursor_position (line_number (c.line ().value + 1),
                                  column_number (0)));

      return s.update (std::move (nb), nc);
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "insert_newline";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return true;
    }
  };
}
