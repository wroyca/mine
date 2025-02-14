#pragma once

#include <cstdlib>

#include <termios.h>

#include <mine/window.hxx>

namespace mine
{
  // Terminal-based window implementation using ANSI escape sequences.
  //
  // Uses raw terminal mode for direct input handling and ANSI escape sequences
  // for text rendering.
  //
  class terminal: public window
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
    // Throws std::runtime_error if terminal setup fails.
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
    // Throws std::runtime_error if terminal attributes cannot be set.
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
    termios orig_termios_;  // original terminal settings
    bool termios_changed_;  // terminal settings modified flag
  };
}
