#include <mine/mine-command-undo.hxx>

using namespace std;

namespace mine
{
  editor_state undo_command::
  execute (const editor_state& s) const
  {
    // Meta-command: implementation is bypassed by editor_core::dispatch
    //
    return s;
  }

  string_view undo_command::
  name () const noexcept
  {
    return "undo";
  }

  bool undo_command::
  modifies_buffer () const noexcept
  {
    return false;
  }
}
