#pragma once

#include <immer/vector.hpp>

namespace mine
{
  // Text buffer implementation.
  //
  class buffer
  {
  public:
    explicit buffer ();

    // Insert text at specified position.
    //
    // Position is specified as line and column numbers (0-based). Throws
    // std::out_of_range if position is invalid.
    //
    void
    insert (std::size_t line, std::size_t column, std::string_view);

    // Erase text at specified position.
    //
    // Position is specified as line and column numbers (0-based). Count
    // specifies how many characters to delete (by default, 1). Throws
    // std::out_of_range if position is invalid or if count exceeds available
    // text.
    //
    void
    erase (std::size_t line, std::size_t column, std::size_t count = 1);

    // Return number of lines in buffer.
    //
    std::size_t
    line_count () const
    {
      return lines.size ();
    }

    // Return line at  specified index.
    //
    // Throws std::out_of_range if line number is invalid.
    //
    const std::string&
    line (std::size_t n) const
    {
      if (n >= lines.size ())
        throw std::out_of_range ("line number out of range");

      return lines.at (n);
    }

  private:
    immer::vector<std::string> lines;
  };
}
