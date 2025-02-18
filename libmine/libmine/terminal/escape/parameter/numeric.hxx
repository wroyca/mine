#pragma once

#include <libmine/export.hxx>

namespace mine::terminal::escape::parameter
{
  // Numeric parameter for terminal escape sequences.
  //
  // Represents a numeric parameter in an escape sequence. Parameters can be
  // explicit or implicit (using default values). For example:
  //
  // ESC[H   -> implicit (defaults to 1,1)
  // ESC[5H  -> explicit (5,1)
  // ESC[;3H -> implicit,explicit (1,3)
  //
  class LIBMINE_SYMEXPORT numeric
  {
  public:
    // Create parameter with no value (will use defaults).
    //
    numeric () = default;

    // Create parameter with specific value.
    //
    explicit
    numeric (uint16_t value);

    // Create parameter from string.
    //
    // Attempts to parse a numeric value from a string. Returns none
    // if the string is not a valid parameter value.
    //
    // For example:
    //
    // "123" -> parameter with value 123
    // "0"   -> parameter with value 0
    // "abc" -> none (invalid)
    //
    static optional<numeric>
    from_string (const std::string& s);

    // Convert parameter to string.
    //
    // Returns string representation of the parameter value, or empty string if
    // using defaults.
    //
    string
    to_string () const;

  // private:
    optional<uint16_t> value;
  };
}

#include <libmine/terminal/escape/parameter/numeric.ixx>
