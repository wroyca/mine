#include <mine/mine-command-quit.hxx>

using namespace std;

namespace mine
{
  editor_state quit_command::
  execute (const editor_state& s) const
  {
    // Meta-command: implementation is bypassed by editor_core::dispatch
    //
    return s;
  }

  string_view quit_command::
  name () const noexcept
  {
    return "quit";
  }

  bool quit_command::
  modifies_buffer () const noexcept
  {
    return false;
  }
}
