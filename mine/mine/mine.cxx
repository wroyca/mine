#include <mine/terminal.hxx>

#ifndef _WIN32
#  include <unistd.h>
#else
#  include <mine/win32-utility.hxx>
#endif

int
main (int argc, char* argv[])
{
  using namespace mine;

#ifdef _WIN32
  terminal_handle h = GetStdHandle (STD_OUTPUT_HANDLE);
#else
  terminal_handle h = STDOUT_FILENO;
#endif

  terminal t (h);
}
