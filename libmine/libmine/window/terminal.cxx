#include <libmine/window/terminal.hxx>
#include <libmine/terminal/csi/cursor/motion.hxx>

#include <iostream>

#include <unistd.h>

#include <sys/ioctl.h>

namespace mine
{
  namespace terminal
  {
    using namespace
    mine::terminal::csi::cursor;

    using namespace
    mine::terminal::escape::parameter;

    terminal::terminal () noexcept
        : termios_changed (false), cursor_line (0), cursor_column (0)
    {
    }

    terminal::~terminal () noexcept { restore_modes (); }

    void
    terminal::run ()
    {
      setup_modes ();

      // Initialize cursor position and clear screen.
      //
      if (!init_cursor_position ())
        throw runtime_error ("unable to initialize cursor position");

      cout << "\033[2J" << position () << flush;
      update_cursor_position (0, 0);

      render ();

      // Main input loop.
      //
      while (true)
      {
        handle_input ();
      }
    }

    bool
    terminal::init_cursor_position ()
    {
      cout << report_position () << flush;

      if (optional<pair<size_t, size_t>> pos = read_cursor_position ())
      {
        cursor_line = pos->first;
        cursor_column = pos->second;
        return true;
      }

      return false;
    }

    void
    terminal::update_cursor_position (size_t line, size_t column)
    {
      cursor_line = line;
      cursor_column = column;
    }

    void
    terminal::render ()
    {
      // Get terminal size.
      //
      winsize w;
      ioctl (STDOUT_FILENO, TIOCGWINSZ, &w);

      // Clear screen and move cursor to top.
      //
      cout << "\033[2J" << position () << flush;

      // Render buffer contents.
      //
      for (size_t i (0); i < b.line_count () && i < w.ws_row; ++i)
      {
        // Move to start of line.
        //
        cout << position (numeric (i + 1), numeric (1));

        const string& l (b.line (i));

        // Truncate line if it exceeds terminal width.
        //
        size_t len (l.size ());
        if (len > w.ws_col)
          len = w.ws_col;

        cout << l.substr (0, len);
      }

      cout << flush;
    }

    void
    terminal::setup_modes ()
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

    void
    terminal::restore_modes () noexcept
    {
      if (termios_changed)
      {
        tcsetattr (STDIN_FILENO, TCSAFLUSH, &orig_termios);
        termios_changed = false;
      }
    }

    // Read cursor position response (DSR - Device Status Report).
    //
    // Response format is ESC[{row};{col}R where row and col are 1-based
    // decimal numbers. Returns 0-based (line, column) pair or nullopt on
    // error.
    //
    optional<pair<size_t, size_t>>
    terminal::read_cursor_position ()
    {
      char buf[32];
      size_t pos (0);

      // Read CSI sequence start (ESC[).
      //
      if (read (STDIN_FILENO, &buf[pos], 1) != 1 || buf[pos++] != '\x1b')
        return nullopt;

      if (read (STDIN_FILENO, &buf[pos], 1) != 1 || buf[pos++] != '[')
        return nullopt;

      // Read response body until 'R' terminator.
      //
      while (pos < sizeof (buf) - 1)
      {
        if (read (STDIN_FILENO, &buf[pos], 1) != 1)
          return nullopt;

        if (buf[pos] == 'R')
        {
          buf[pos] = '\0';
          break;
        }

        pos++;
      }

      // Parse response into row and column components.
      //
      string resp (buf + 2); // skip CSI
      size_t semi (resp.find (';'));
      if (semi == string::npos)
        return nullopt;

      optional<numeric> row (numeric::from_string (resp.substr (0, semi)));
      optional<numeric> col (numeric::from_string (resp.substr (semi + 1)));

      if (!row || !col)
        return nullopt;

      // Convert from 1-based response to 0-based indices.
      //
      return make_pair (row->value.value () - 1, col->value.value () - 1);
    }

    void
    terminal::handle_input ()
    {
      char c;
      if (read (STDIN_FILENO, &c, 1) == 1)
      {
        switch (c)
        {
        // Quit command. Restore terminal and exit cleanly.
        //
        case 'q':
          restore_modes ();
          exit (0);
          break;

        // CSI sequence. Handle arrow keys and other escape sequences.
        //
        case '\x1b':
          if (!handle_escape_sequence ())
            render (); // redraw on unhandled sequence
          break;

        // Enter key. Insert newline at current position and move cursor to
        // beginning of next line.
        //
        case '\r':
        case '\n':
          {
            b.insert (cursor_line, cursor_column, "\n");
            cout << down (numeric (1)) << position (numeric (), numeric (1))
                 << flush;
            update_cursor_position (cursor_line + 1, 0);
            render ();
            break;
          }

        // Backspace. Remove character before cursor, handling line joining if
        // at start of line.
        //
        case 127:
          {
            if (cursor_column > 0)
            {
              b.erase (cursor_line, cursor_column - 1);
              cout << backward (numeric (1)) << flush;
              update_cursor_position (cursor_line, cursor_column - 1);
            }
            else if (cursor_line > 0)
            {
              size_t pl (cursor_line - 1);     // previous line
              size_t pc (b.line (pl).size ()); // previous column

              b.erase (pl, pc);
              cout << up (numeric (1))
                   << position (numeric (pl + 1), numeric (pc + 1)) << flush;
              update_cursor_position (pl, pc);
            }
            render ();
            break;
          }

        // Printable character. Insert at current position.
        //
        default:
          {
            if (c >= 32 && c < 127)
            {
              b.insert (cursor_line, cursor_column, string (1, c));
              cout << c << flush;
              update_cursor_position (cursor_line, cursor_column + 1);
            }
            break;
          }
        }
      }
    }

    bool
    terminal::handle_escape_sequence ()
    {
      // @@ TODO
      //
      // Current implementation has several architectural issues that need to
      // be addressed:
      //
      // 1. We currently re-encode and send back the same CSI sequences we
      //    receive instead of handling them directly (see 2. below).
      //
      // 2. No proper infrastructure exists for receiving and parsing arbitrary
      //    escape sequences (C0, C1, CSI, etc).
      //
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
        // Move cursor up one line maintaining horizontal position if possible.
        // If target line is shorter, position cursor at its end.
        //
        case 'A':
          {
            if (cursor_line > 0)
            {
              size_t tl (cursor_line - 1);     // target line
              size_t ll (b.line (tl).size ()); // line length

              if (cursor_column <= ll)
              {
                cout << up (numeric (1)) << flush;
                update_cursor_position (tl, cursor_column);
              }
              else
              {
                // Adjust to end of shorter line to maintain buffer bounds.
                //
                cout << up (numeric (1))
                     << position (numeric (tl + 1), numeric (ll + 1)) << flush;
                update_cursor_position (tl, ll);
              }
            }
            return true;
          }

        // Move cursor down one line maintaining horizontal position if
        // possible. If target line is shorter, position cursor at its end.
        //
        case 'B':
          {
            if (cursor_line < b.line_count () - 1)
            {
              size_t tl (cursor_line + 1);     // target line
              size_t ll (b.line (tl).size ()); // line length

              if (cursor_column <= ll)
              {
                cout << down (numeric (1)) << flush;
                update_cursor_position (tl, cursor_column);
              }
              else
              {
                // Adjust to end of shorter line to maintain buffer bounds.
                //
                cout << down (numeric (1))
                     << position (numeric (tl + 1), numeric (ll + 1)) << flush;
                update_cursor_position (tl, ll);
              }
            }
            return true;
          }

        // Move cursor one position right unless at end of line.
        //
        case 'C':
          {
            if (cursor_column < b.line (cursor_line).size ())
            {
              cout << forward (numeric (1)) << flush;
              update_cursor_position (cursor_line, cursor_column + 1);
            }
            return true;
          }

        // Move cursor one position left unless at start of line.
        //
        case 'D':
          {
            if (cursor_column > 0)
            {
              cout << backward (numeric (1)) << flush;
              update_cursor_position (cursor_line, cursor_column - 1);
            }
            return true;
          }
        }
      }

      return false;
    }
  }
}
