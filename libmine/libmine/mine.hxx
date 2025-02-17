#pragma once

#include <iosfwd>
#include <string>

#include <libmine/export.hxx>

namespace mine
{
  // Print a greeting for the specified name into the specified
  // stream. Throw std::invalid_argument if the name is empty.
  //
  LIBMINE_SYMEXPORT void
  say_hello (std::ostream&, const std::string& name);
}
