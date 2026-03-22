#pragma once

#include <mine/mine-command-base.hxx>

#include <mine/mine-clipboard.hxx>
#include <mine/mine-unicode-grapheme.hxx>

namespace mine
{
  class copy_command : public command
  {
  public:
    editor_state
    execute (const editor_state& s) const override
    {
      auto c (s.get_cursor ());

      // See if we actually have an active selection. Note that if the mark is
      // at the current position, it doesn't really count.
      //
      if (c.has_mark () && c.mark () != c.position ())
      {
        auto p1 (std::min (c.position (), c.mark ()));
        auto p2 (std::max (c.position (), c.mark ()));

        // Step the end forward. Cell highlighting is inclusive so we need to
        // grab the character under the cursor too.
        //
        auto e (cursor (p2).move_right (s.buffer ()).position ());

        std::string t (s.buffer ().get_range (p1, e));
        set_clipboard_text (t);

        // Clear the mark. This gives the user visual feedback that the text was
        // actually copied.
        //
        c.clear_mark ();

        return s.with_cursor (c);
      }

      return s;
    }

    std::string_view
    name () const noexcept override
    {
      return "copy";
    }

    bool
    modifies_buffer () const noexcept override
    {
      return false;
    }
  };

  class paste_command : public command
  {
  public:
    editor_state
    execute (const editor_state& s) const override
    {
      // Grab the clipboard text. Bail out early if there is nothing
      // to do.
      //
      std::string t (get_clipboard_text ());
      if (t.empty ())
        return s;

      auto b (s.buffer ());
      auto c (s.get_cursor ());

      // If there is an active selection, pasting should just blast
      // over it.
      //
      if (c.has_mark () && c.mark () != c.position ())
      {
        auto p1 (std::min (c.position (), c.mark ()));
        auto p2 (std::max (c.position (), c.mark ()));
        auto e (cursor (p2).move_right (b).position ());

        b = b.delete_range (p1, e);
        c = c.move_to (p1);
      }

      c.clear_mark ();

      // Shovel the clipboard text into the buffer. We have to be careful here
      // and handle newlines manually since the text might span multiple lines.
      //
      std::size_t st (0);
      std::size_t p (t.find ('\n'));

      while (p != std::string::npos)
      {
        std::string ck (t.substr (st, p - st));

        if (!ck.empty ())
        {
          b = b.insert_graphemes (c.position (), ck);
          std::size_t n (count_graphemes (ck));

          c = c.move_to (
            cursor_position (c.line (), column_number (c.column ().value + n)));
        }

        b = b.insert_newline (c.position ());
        c = c.move_to (cursor_position (line_number (c.line ().value + 1),
                                        column_number (0)));

        st = p + 1;
        p = t.find ('\n', st);
      }

      // Don't forget any trailing text after the last newline.
      //
      if (st < t.size ())
      {
        std::string ck (t.substr (st));
        b = b.insert_graphemes (c.position (), ck);
        std::size_t n (count_graphemes (ck));

        c = c.move_to (
          cursor_position (c.line (), column_number (c.column ().value + n)));
      }

      return s.update (std::move (b), c);
    }

    std::string_view
    name () const noexcept override
    {
      return "paste";
    }

    bool
    modifies_buffer () const noexcept override
    {
      return true;
    }
  };
}
