#include <libmine/window.hxx>
#include <libmine/window/gtk.hxx>
#include <libmine/window/terminal.hxx>

using namespace std;

namespace mine
{
  unique_ptr <window>
  create_window (window_type t)
  {
    switch (t)
    {
    case window_type::gtk:
      return make_unique<gtk>();
    case window_type::terminal:
      return make_unique<terminal::terminal>();
    }
    throw runtime_error ("unknown window type");
  }
}
