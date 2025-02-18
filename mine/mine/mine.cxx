#include <libmine/mine.hxx>
#include <libmine/version.hxx>
#include <libmine/window.hxx>

#include <print>
#include <iostream>

using namespace std;

namespace mine
{
  namespace
  {
    void
    main (int argc, char* argv[])
    {
      options ops;

      // Process command line options and arguments.
      //
      try
      {
        ops.parse (argc, argv);

        if (ops.help ())
        {
          ops.print_usage (cout);
          return;
        }

        if (ops.version ())
        {
          println ("mine {}", LIBMINE_VERSION_STR);
          return;
        }

        create_window (ops.no_window() ? window_type::terminal
                                       : window_type::gtk)->run();
      }

      // Handle unknown options.
      //
      // Note: CLI's handling of single-dash options has an important
      // implication for error reporting. When an invalid option like "-help"
      // is encountered, e.option() returns only its first character as "-h",
      // not the complete "-help" string.
      //
      // While we could attempt to reconstruct the original option by matching
      // "-h" against known options (e.g., --help), this approach becomes
      // problematic with similar options. For instance, if both --help and
      // --heap were valid options, it would be difficult to determine whether
      // -h was meant to be -help or -heap.
      //
      // @@ Another implication is that we can't use option string to generate
      // "did you mean" diagnostics (e.g., "unknown option '-help', did you
      // mean '--help'?").
      //
      catch (const cli::unknown_option& e)
      {
        println (cerr, "{}: {}", e.what (), e.option ());
        exit (1);
      }

      // Handle all other exceptions.
      //
      catch (const cli::exception& e)
      {
        println (cerr, "{}", e.what ());
        exit (1);
      }
    }
  }
}

int
main (int argc, char* argv[])
{
  mine::main (argc, argv);
}
