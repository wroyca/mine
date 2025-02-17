#pragma once

#include <iosfwd>
#include <string>

#include <libmine/terminal/export.hxx>

namespace mine_terminal
{
  // Print a greeting for the specified name into the specified
  // stream. Throw std::invalid_argument if the name is empty.
  //
  LIBMINE_TERMINAL_SYMEXPORT void
  say_hello (std::ostream&, const std::string& name);
}
