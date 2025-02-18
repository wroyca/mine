#include <libmine/window/terminal.hxx>

#include <iostream>

#include <unistd.h>

#include <sys/ioctl.h>

namespace mine
{
  terminal::
  terminal () noexcept
    : termios_changed (false),
      cursor_line (0),
      cursor_column (0)
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

    // Save cursor position.
    //
    cout << "\033[s";

    // Clear screen and move cursor to top.
    //
    cout << "\033[2J\033[H";

    // Render buffer contents.
    //
    for (size_t i (0); i < b.line_count () && i < w.ws_row; ++i)
    {
      // Move to start of line.
      //
      cout << "\033[" << (i + 1) << ";1H";

      const string& l (b.line (i));

      // Truncate line if it exceeds terminal width.
      //
      size_t len (l.size ());
      if (len > w.ws_col)
        len = w.ws_col;

      cout << l.substr (0, len);
    }

    // Position cursor.
    //
    cout << "\033[" << (cursor_line + 1) << ";" << (cursor_column + 1) << "H";
    cout << flush;
  }

  void terminal::
  setup_modes ()
  {
    // Get current terminal settings.
    //
    if (tcgetattr (STDIN_FILENO, &orig_termios) == -1)
      throw runtime_error ("unable to get terminal attributes");

    // Modify terminal settings.
    //
    termios raw (orig_termios);

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

    termios_changed = true;
  }

  void terminal::
  restore_modes () noexcept
  {
    if (termios_changed)
    {
      tcsetattr (STDIN_FILENO, TCSAFLUSH, &orig_termios);
      termios_changed = false;
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

      case '\r':
      case '\n':
        {
          b.insert (cursor_line, cursor_column, "\n");
          cursor_line++;
          cursor_column = 0;
          render ();
          break;
        }

      case 127: // backspace
        {
          if (cursor_column > 0)
          {
            b.erase (cursor_line, cursor_column - 1);
            cursor_column--;
            render ();
          }
          else if (cursor_line > 0)
          {
            cursor_line--;
            cursor_column = b.line (cursor_line).size ();
            b.erase (cursor_line, cursor_column);
            render ();
          }
          break;
        }

      default:
        {
          if (c >= 32 && c < 127)
          {
            b.insert (cursor_line, cursor_column, string (1, c));
            cursor_column++;
            render ();
          }
          break;
        }
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
      // Up arrow.
      //
      case 'A':
        if (cursor_line > 0)
        {
          cursor_line--;
          if (cursor_column > b.line(cursor_line).size())
            cursor_column = b.line(cursor_line).size();
          render();
        }
        return true;

      // Down arrow.
      //
      case 'B':
        if (cursor_line < b.line_count() - 1)
        {
          cursor_line++;
          if (cursor_column > b.line(cursor_line).size())
            cursor_column = b.line(cursor_line).size();
          render();
        }
        return true;

      // Right arrow.
      //
      case 'C':
        if (cursor_column < b.line(cursor_line).size())
        {
          cursor_column++;
          render();
        }
        return true;

      // Left arrow.
      //
      case 'D':
        if (cursor_column > 0)
        {
          cursor_column--;
          render();
        }
        return true;
      }
    }

    return false;
  }
}
