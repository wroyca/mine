#pragma once

#include <string_view>

#include <mine/mine-assert.hxx>

namespace mine
{
  class opengl_context
  {
  public:
    opengl_context ();

    // Queries.
    //

    std::string_view
    version () const;

    std::string_view
    renderer () const;

    std::string_view
    vendor () const;

    bool
    has_extension (std::string_view) const;
  };
}
