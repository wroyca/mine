#include <mine/window/terminal.hxx>

#include <iostream>

#include <unistd.h>

#include <sys/ioctl.h>

using namespace std;

namespace mine
{
  terminal::
  terminal () noexcept
    : termios_changed_ (false)
  {
  }

  terminal::
  ~terminal () noexcept
  {
    restore_modes ();
  }

  void terminal::
  run ()
  {
    setup_modes ();

    // Clear screen and move cursor to top.
    //
    cout << "\033[2J\033[H" << flush;

    render ();

    // Main input loop.
    //
    while (true)
    {
      handle_input ();
    }
  }

  void terminal::
  render ()
  {
    // Get terminal size.
    //
    winsize w;
    ioctl (STDOUT_FILENO, TIOCGWINSZ, &w);

    // Clear screen and move cursor to top.
    //
    cout << "\033[2J\033[H";

    // Split content into lines and render.
    //
    const string content ("hello");
    size_t pos (0);
    size_t line (0);

    while (pos < content.size () && line < w.ws_row)
    {
      size_t next (content.find ('\n', pos));
      if (next == string::npos)
        next = content.size ();

      // Truncate line if it exceeds terminal width.
      //
      size_t len (next - pos);
      if (len > w.ws_col)
        len = w.ws_col;

      cout << content.substr (pos, len) << '\n';

      pos = next + 1;
      line++;
    }

    cout << flush;
  }

  void terminal::
  setup_modes ()
  {
    // Get current terminal settings.
    //
    if (tcgetattr (STDIN_FILENO, &orig_termios_) == -1)
      throw runtime_error ("unable to get terminal attributes");

    // Modify terminal settings.
    //
    struct termios raw (orig_termios_);

    // Input modes: no break, no CR to NL, no parity check, no strip char,
    // no start/stop output control.
    //
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // Output modes - disable post processing.
    //
    raw.c_oflag &= ~(OPOST);

    // Control modes - set 8 bit chars.
    //
    raw.c_cflag |= (CS8);

    // Local modes - echoing off, canonical off, no extended functions,
    // no signal chars (^Z,^C).
    //
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // Control chars - set return condition: min number of bytes and timer.
    //
    raw.c_cc[VMIN] = 0;  // return each byte, or 0 for timeout
    raw.c_cc[VTIME] = 1; // 100 ms timeout

    // Put terminal in raw mode.
    //
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &raw) == -1)
      throw runtime_error ("unable to set terminal attributes");

    termios_changed_ = true;
  }

  void terminal::
  restore_modes () noexcept
  {
    if (termios_changed_)
    {
      tcsetattr (STDIN_FILENO, TCSAFLUSH, &orig_termios_);
      termios_changed_ = false;
    }
  }

  void terminal::
  handle_input ()
  {
    char c;
    if (read (STDIN_FILENO, &c, 1) == 1)
    {
      switch (c)
      {
      case 'q':
        // Exit on 'q'.
        //
        restore_modes ();
        exit (0);
        break;

      case '\x1b':
        // Handle escape sequences (arrow keys, etc).
        //
        if (!handle_escape_sequence ())
          render (); // redraw on unknown sequence
        break;

      default:
        render (); // redraw on any other input
        break;
      }
    }
  }

  bool terminal::
  handle_escape_sequence ()
  {
    char seq[3];

    // Read up to 2 more bytes of the sequence.
    //
    if (read (STDIN_FILENO, &seq[0], 1) != 1)
      return false;

    if (read (STDIN_FILENO, &seq[1], 1) != 1)
      return false;

    if (seq[0] == '[')
    {
      switch (seq[1])
      {
      // Up arrow.,
      case 'A':
        return true;

      // Down arrow.
      case 'B':
        return true;

      // Right arrow.
      case 'C':
        return true;

      // Left arrow.
      case 'D':
        return true;
      }
    }

    return false;
  }
}
