#include <mine/mine-command-insert.hxx>

using namespace std;

namespace mine
{
  insert_text_command::
  insert_text_command (string text)
    : text_ (move (text))
  {
  }

  editor_state insert_text_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, typing replaces the marked
    // region. First, we clear out the selected text.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

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
    // Since we assume the text contains no newlines (that is, it's strictly a
    // horizontal insertion), we don't need to recalculate the line. We can
    // simply advance the column index by the number of graphemes we just
    // inserted.
    //
     const size_t n (count_graphemes (text_));

    c = c.move_to (
      cursor_position (c.line (), column_number (c.column ().value + n)));

    return s.update (move (b), c);
  }

  string_view insert_text_command::
  name () const noexcept
  {
    return "insert_text";
  }

  bool insert_text_command::
  modifies_buffer () const noexcept
  {
    return true;
  }

  editor_state insert_newline_command::
  execute (const editor_state& s) const
  {
    auto b (s.buffer ());
    auto c (s.get_cursor ());

    // See if we have an active selection. If so, hitting enter replaces the
    // marked region. First, we drop the existing selection.
    //
    if (c.has_mark () && c.mark () != c.position ())
    {
      auto p1 (min (c.position (), c.mark ()));
      auto p2 (max (c.position (), c.mark ()));

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
    c = c.move_to (
      cursor_position (line_number (c.line ().value + 1), column_number (0)));

    return s.update (move (b), c);
  }

  string_view insert_newline_command::
  name () const noexcept
  {
    return "insert_newline";
  }

  bool insert_newline_command::
  modifies_buffer () const noexcept
  {
    return true;
  }
}
