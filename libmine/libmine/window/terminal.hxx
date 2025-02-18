#pragma once

#include <termios.h>

#include <libmine/window.hxx>
#include <libmine/buffer.hxx>
#include <libmine/export.hxx>

namespace mine
{
  // Terminal-based window implementation using ANSI escape sequences.
  //
  // Uses raw terminal mode for direct input handling and ANSI escape sequences
  // for text rendering.
  //
  class LIBMINE_SYMEXPORT terminal: public window
  {
  public:
    // Create terminal window.
    //
    explicit
    terminal () noexcept;

    // Destroy terminal window. Restores terminal settings.
    //
    ~terminal () noexcept override;

    // Run terminal event loop. Blocks until 'q' is pressed.
    //
    // Throws runtime_error if terminal setup fails.
    //
    void
    run () override;

    // Render content using ANSI escape sequences.
    //
    void
    render ();

  private:
    // Set up raw terminal mode.
    //
    // Throws runtime_error if terminal attributes cannot be set.
    //
    void
    setup_modes ();

    // Restore original terminal settings.
    //
    void
    restore_modes () noexcept;

    // Handle terminal input.
    //
    void
    handle_input ();

    // Handle ANSI escape sequences (arrow keys, etc).
    //
    // Returns true if sequence was handled.
    //
    bool
    handle_escape_sequence ();

  private:
    termios orig_termios;  // original terminal settings
    bool termios_changed;  // terminal settings modified flag
    buffer b;
    size_t cursor_line;
    size_t cursor_column;
  };
}
