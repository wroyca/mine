#pragma once

#include <mine/mine-core-state.hxx>

namespace mine
{
  class window_renderer
  {
  public:
    // We initialize any required GL state here.
    //
    window_renderer ();

    // Draw the current editor state.
    //
    void
    render (const editor_state& s);

    // Update the viewport dimensions.
    //
    void
    resize (int w, int h);
  };
}
