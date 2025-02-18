#pragma once

#include <memory>

#include <libmine/export.hxx>

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

  class LIBMINE_SYMEXPORT window
  {
  public:
    virtual
    ~window () = default;

    // Run window's event loop. Blocks until window is closed.
    //
    virtual void
    run () = 0;
  };

  // Factory function for creating window implementations.
  //
  LIBMINE_SYMEXPORT std::unique_ptr <window>
  create_window (window_type);
}
