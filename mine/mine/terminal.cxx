#include <mine/terminal.hxx>

#ifndef _WIN32
#  include <termios.h>
#else
#  include <mine/win32-utility.hxx>
#endif

#include <cstdlib>
#include <iostream>

using namespace std;

namespace mine
{
  namespace
  {
    termios
    get_terminal_attributes (int fd)
    {
      termios attrs;

      if (tcgetattr (fd, &attrs) != 0)
        throw terminal_error (errno);

      return attrs;
    }

    void
    set_terminal_attributes (int fd, const termios& attrs)
    {
      if (tcsetattr (fd, TCSANOW, &attrs) != 0)
        throw terminal_error (errno);
    }

    terminal_characteristics
    termios_to_characteristics (const termios& attrs, int fd)
    {
      terminal_characteristics tc;

      if (attrs.c_lflag & ICANON)
        tc.input_mode = terminal_input_mode::canonical;
      else if (attrs.c_lflag & ISIG)
        tc.input_mode = terminal_input_mode::cbreak;
      else
        tc.input_mode = terminal_input_mode::raw;

      return tc;
    }

    struct termios
    characteristics_to_termios (const terminal_characteristics& tc,
                                const struct termios& original)
    {
      termios attrs = original;

      switch (tc.input_mode)
      {
      case terminal_input_mode::canonical:
          break;

      case terminal_input_mode::raw:
        {
          attrs.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                             IGNCR | ICRNL | IXON);
          attrs.c_oflag &= ~OPOST;
          attrs.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
          attrs.c_cflag &= ~(CSIZE | PARENB);
          attrs.c_cflag |= CS8;
          attrs.c_cc[VMIN] = 1;
          attrs.c_cc[VTIME] = 0;

          break;
        }

      case terminal_input_mode::cbreak:
          break;
      }

      return attrs;
    }
  }

  //
  //
  //

  template <> terminal_characteristics terminal_traits<terminal_handle>::
  get_characteristics (const terminal_handle& h)
  {
    return termios_to_characteristics (get_terminal_attributes (h), h);
  }

  template <> void terminal_traits<terminal_handle>::
  set_characteristics (const terminal_handle& h, const terminal_characteristics& tc)
  {
    set_terminal_attributes (h, characteristics_to_termios (tc, get_terminal_attributes (h)));
  }

  //
  //
  //

  template <typename H, typename T> basic_terminal<H, T>::
  basic_terminal (handle_type h)
    : handle_ (std::move (h))
  {
  }

  template <typename H, typename T> terminal_characteristics basic_terminal<H, T>::
  characteristics () const
  {
    return traits_type::get_characteristics (handle_);
  }

  template <typename H, typename T> void basic_terminal<H, T>::
  characteristics (const terminal_characteristics& tc)
  {
    traits_type::set_characteristics (handle_, tc);
  }

  template <typename H, typename T> void basic_terminal<H, T>::
  set_input_mode (terminal_input_mode mode)
  {
    terminal_characteristics tc (characteristics ());
    tc.input_mode = mode;
    characteristics (tc);
  }

  template <typename H, typename T> terminal_input_mode basic_terminal<H, T>::
  input_mode () const
  {
    return characteristics ().input_mode;
  }

  template <typename H, typename T> void basic_terminal<H, T>::
  set_raw_mode ()
  {
    if (!original_characteristics_)
      save_original_characteristics ();

    set_input_mode (terminal_input_mode::raw);
  }

  template <typename H, typename T> void basic_terminal<H, T>::
  save_original_characteristics ()
  {
    try
    {
      original_characteristics_ = characteristics ();
    }
    catch (const terminal_error&)
    {
      // Note that while failing to save the original terminal characteristics
      // is not strictly a fatal error (the editor could continue to operate as
      // long as we can still modify terminal characteristics), in practice this
      // failure is indicative of deeper terminal access issues.
      //
      // If we cannot save the current state, we likely cannot set a new state
      // later and even if we *do* succeed in setting new characteristics, the
      // inability to restore the original state creates an unreliable user
      // experience that is unacceptable for production use.
      //
      // Therefore, we treat this as a hard error and terminate early to avoid
      // leaving the user's terminal in an unknown state.
      //
      cerr << "error: failed to save terminal characteristics" << "\n";
      exit (1);
    }
  }

  template <typename H, typename T> void basic_terminal<H, T>::
  restore_original_characteristics ()
  {
    if (original_characteristics_)
    {
      try
      {
        characteristics (*original_characteristics_);
      }
      catch (const terminal_error&)
      {
        cerr << "warning: failed to restore terminal characteristics" << "\n";
      }
    }
  }

  // Explicit template instantiation for the default terminal type.
  //
  template class basic_terminal<terminal_handle>;
}
