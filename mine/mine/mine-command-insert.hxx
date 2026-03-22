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
      auto b (s.buffer ());
      auto c (s.get_cursor ());

      // See if we have an active selection. If so, typing replaces the
      // marked region. First, we clear out the selected text.
      //
      if (c.has_mark () && c.mark () != c.position ())
      {
        auto p1 (std::min (c.position (), c.mark ()));
        auto p2 (std::max (c.position (), c.mark ()));

        // Step the end boundary forward to account for inclusive selection.
        //
        auto e (cursor (p2).move_right (b).position ());

        b = b.delete_range (p1, e);
        c = c.move_to (p1);
      }

      c.clear_mark ();

      // Sanity check: verify the input layer gave us valid UTF-8. It really
      // should have, but better safe than sorry before we commit to the buffer.
      //
      assert_valid_utf8 (text_);

      // Now inject the grapheme sequence into the buffer right at the current
      // cursor position.
      //
      b = b.insert_graphemes (c.position (), text_);

      // Figure out where the cursor ends up.
      //
      // Since we assume the text contains no newlines (that is, it's strictly
      // a horizontal insertion), we don't need to recalculate the line. We can
      // simply advance the column index by the number of graphemes we just
      // inserted.
      //
      std::size_t n (count_graphemes (text_));

      c = c.move_to (
        cursor_position (c.line (), column_number (c.column ().value + n)));

      return s.update (std::move (b), c);
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
  class insert_newline_command : public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      auto b (s.buffer ());
      auto c (s.get_cursor ());

      // See if we have an active selection. If so, hitting enter replaces the
      // marked region. First, we drop the existing selection.
      //
      if (c.has_mark () && c.mark () != c.position ())
      {
        auto p1 (std::min (c.position (), c.mark ()));
        auto p2 (std::max (c.position (), c.mark ()));

        // Step the end boundary forward to account for inclusive selection.
        //
        auto e (cursor (p2).move_right (b).position ());

        b = b.delete_range (p1, e);
        c = c.move_to (p1);
      }

      c.clear_mark ();

      // Now actually split the line in the buffer right at our current
      // cursor position.
      //
      b = b.insert_newline (c.position ());

      // Finally, advance the cursor down to the start of the next line.
      //
      c = c.move_to (cursor_position (line_number (c.line ().value + 1),
                                      column_number (0)));

      return s.update (std::move (b), c);
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
