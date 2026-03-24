#include <mine/mine-command-redo.hxx>

using namespace std;

namespace mine
{
  editor_state redo_command::
  execute (const editor_state& s) const
  {
    // Meta-command: implementation is bypassed by editor_core::dispatch.
    //
    return s;
  }

  string_view redo_command::
  name () const noexcept
  {
    return "redo";
  }

  bool redo_command::
  modifies_buffer () const noexcept
  {
    return false;
  }
}
