#pragma once

#include <string>
#include <utility>

#include <mine/mine-command-base.hxx>
#include <mine/mine-unicode-assert.hxx>
#include <mine/mine-unicode-grapheme.hxx>

namespace mine
{
  // Insert a span of text at the current cursor position.
  //
  // Note that this command expects a "flat" string of text (no control
  // characters or newlines). We assume that the input layer has already
  // stripped or processed those, routing <Enter> to insert_newline_command,
  // for example.
  //
  class insert_text_command: public command
  {
  public:
    explicit
    insert_text_command (std::string text)
      : text_ (std::move (text))
    {
    }

    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Sanity check: the input layer gave us valid UTF-8.
      //
      assert_valid_utf8 (text_);

      // Insert the grapheme sequence into the buffer at the current position.
      //
      auto nb (b.insert_graphemes (c.position (), text_));

      // Calculate the new cursor position.
      //
      // Since we assume the text contains no newlines (it's a horizontal
      // insertion), we can simply advance the column index by the number of
      // graphemes inserted.
      //
      std::size_t n (count_graphemes (text_));
      cursor nc (cursor_position (c.line (),
                                  column_number (c.column ().value + n)));

      return s.update (std::move (nb), nc);
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "insert_text";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return true;
    }

  private:
    std::string text_;
  };

  // Insert a logical newline (break the current line).
  //
  // This operation splits the current line at the cursor position and moves
  // the cursor to the beginning (column 0) of the newly created line.
  //
  class insert_newline_command: public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      const auto& b (s.buffer ());
      const auto& c (s.get_cursor ());

      // Split the line in the buffer.
      //
      auto nb (b.insert_newline (c.position ()));

      // Move the cursor to the start of the next line.
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
