#pragma once

#include <immer/vector.hpp>

#include <libmine/export.hxx>

namespace mine
{
  // Text buffer implementation.
  //
  class LIBMINE_SYMEXPORT buffer
  {
  public:
    explicit buffer ();

    // Insert text at specified position.
    //
    // Position is specified as line and column numbers (0-based). Throws
    // out_of_range if position is invalid.
    //
    void
    insert (size_t line, size_t column, string_view);

    // Erase text at specified position.
    //
    // Position is specified as line and column numbers (0-based). Count
    // specifies how many characters to delete (by default, 1). Throws
    // out_of_range if position is invalid or if count exceeds available text.
    //
    void
    erase (size_t line, size_t column, size_t count = 1);

    // Return number of lines in buffer.
    //
    size_t
    line_count () const
    {
      return lines.size ();
    }

    // Return line at  specified index.
    //
    // Throws out_of_range if line number is invalid.
    //
    const string&
    line (size_t n) const
    {
      if (n >= lines.size ())
        throw out_of_range ("line number out of range");

      return lines.at (n);
    }

  private:
    immer::vector<string> lines;
  };
}
