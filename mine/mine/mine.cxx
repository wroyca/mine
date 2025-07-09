#include <mine/terminal.hxx>

#include <iostream>

#ifndef _WIN32
#  include <unistd.h>
#else
#  include <mine/win32-utility.hxx>
#endif

int
main (int argc, char* argv[])
{
  using namespace mine;
  using namespace std;

#ifdef _WIN32
  terminal_handle h = GetStdHandle (STD_OUTPUT_HANDLE);
#else
  terminal_handle h = STDOUT_FILENO;
#endif

  terminal t (h);

  t.set_raw_mode();

  cout << "Raw mode test - press 'q' to quit, any other key to echo\r\n";
  cout.flush();

  char c;
  while (true)
  {
    if (read(STDIN_FILENO, &c, 1) != 1)
      break;

    if (c == 'q' || c == 'Q')
      break;

    cout << "You pressed: " << c << "\r\n";
    cout.flush();
  }
}
