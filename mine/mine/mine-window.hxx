#pragma once

#include <memory>

namespace mine
{
  // The term "window" is used broadly here to refer to any visual
  // container for displaying and editing content.
  //
  // This includes both traditional GUI windows (e.g., GTK) and terminal-based
  // displays that take over the entire terminal screen.

  // Tag for window(s).
  //
  enum class window_type
  {
    gtk,
    terminal,
  };

  class window
  {
  public:
    virtual
    ~window () = default;

    // Run window's event loop. Blocks until window is closed.
    //
    virtual void
    run () = 0;
  };
}
