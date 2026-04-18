#include <mine/mine-file-editor.hxx>

using namespace std;

namespace mine
{
  pair<file_editor, file_io_effect> file_editor::
  load (const string& fn) const
  {
    auto [nb, eff] = load_file (file_doc_, fn);

    auto ns (workspace ().with_content (nb.text).with_modified (false));

    return {
      file_editor (move (nb), move (ns)),
      move (eff)
    };
  }

  pair<file_editor, file_io_effect> file_editor::
  save () const
  {
    auto [nb, eff] = save_file (file_doc_);

    auto ns (editor_state_.with_modified (false));

    return {
      file_editor (move (nb), move (ns)),
      move (eff)
    };
  }

  file_editor file_editor::
  update_with_action (file_io_action a) const
  {
    auto [nb, msg] = update_file_document (file_doc_, a);

    auto ns (editor_state_);

    if (nb.text != file_doc_.text)
      ns = ns.with_content (nb.text).with_modified (false);

    auto r (file_editor (move (nb), move (ns)));
    r.message_ = move (msg);

    return r;
  }

  file_editor file_editor::
  update_with_state (workspace ns) const
  {
    auto r (*this);

    if (ns.active_content () != editor_state_.active_content ())
      r.file_doc_.text = ns.active_content ();

    r.editor_state_ = move (ns);
    return r;
  }
}
