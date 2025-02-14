#include <iostream>
#include <memory>
#include <concepts>

#include <mine/window.hxx>

using namespace std;

namespace mine
{
  static void
  usage (ostream& o)
  {
    o << "usage: mine [--nw]" << "\n"
      << "options:" << "\n"
      << "  --nw  use terminal interface" << "\n";
  }
}

int
main (int argc, char* argv[])
{
  using namespace mine;

  // Parse command line options.
  //
  bool terminal (false);

  for (int i (1); i < argc; ++i)
  {
    string a (argv[i]);

    if (a == "--nw")
      terminal = true;
    else
    {
      cerr << "error: unknown option '" << a << "'" << "\n";
      usage (cerr);
      return EXIT_FAILURE;
    }
  }

  auto w (create_window (terminal ? window_type::terminal
                                  : window_type::gtk));
  w->run ();
}
