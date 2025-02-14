#include <mine/window.hxx>
#include <mine/window/gtk.hxx>
#include <mine/window/terminal.hxx>

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
      return make_unique<terminal>();
    }
    throw runtime_error ("unknown window type");
  }
}
