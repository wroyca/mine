#include <mine/terminal.hxx>

#ifndef _WIN32
#  include <termios.h>
#else
#  include <mine/win32-utility.hxx>
#endif

#include <cstdlib>

namespace mine
{
  template <typename H, typename T> basic_terminal<H, T>::
  basic_terminal (handle_type h)
    : handle_ (std::move (h))
  {
  }

  // Explicit template instantiation for the default terminal type.
  //
  template class basic_terminal<terminal_handle>;
}
