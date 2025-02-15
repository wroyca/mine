#include <mine/buffer.hxx>

using namespace std;

namespace mine
{
  buffer::
  buffer ()
    : lines (immer::vector<std::string>{std::string{}})
  {
  }

  void buffer::
  insert (size_t line, size_t column, string_view s)
  {
    if (line >= lines.size ())
      throw out_of_range ("insert: line number out of range");

    // Handle newlines.
    //
    if (s == "\n")
    {
      string current (lines.at (line));
      string next_line;

      if (column < current.size ())
      {
        // Text after cursor goes to new line.
        //
        next_line = current.substr (column);

        // keep text before cursor.
        //
        current.resize (column);
      }

      // Create new lines vector with split.
      //
      auto new_lines (lines.take (line));
      new_lines = move (new_lines).push_back (move (current));
      new_lines = move (new_lines).push_back (move (next_line));

      // Add remaining lines.
      //
      for (size_t i (line + 1); i < lines.size (); ++i)
        new_lines = move (new_lines).push_back (lines.at (i));

      lines = move (new_lines);
      return;
    }

    // Handle regular insertion.
    //
    string l (lines.at (line));

    if (column > l.size ())
      throw out_of_range ("insert: column number out of range");

    l.insert (column, s);
    lines = move (lines).set (line, move (l));
  }

  void buffer::
  erase (size_t line, size_t column, size_t count)
  {
    if (line >= lines.size ())
      throw out_of_range ("erase: line number out of range");

    // Handle regular erase.
    //
    string l (lines.at (line));

    // Handle newlines.
    //
    if (column >= l.size ())
    {
      // At EOL, try to join with next line.
      //
      if (column == l.size () && line < lines.size () - 1)
      {
        l.append (lines.at (line + 1));

        auto new_lines (lines.take (line));
        new_lines = move (new_lines).push_back (move (l));

        for (size_t i (line + 2); i < lines.size (); ++i)
          new_lines = move (new_lines).push_back (lines.at (i));

        lines = move (new_lines);
        return;
      }
      throw out_of_range ("erase: column number out of range");
    }

    if (column + count > l.size ())
      throw out_of_range ("erase: count exceeds available text");

    l.erase (column, count);
    lines = move (lines).set (line, move (l));
  }
}
