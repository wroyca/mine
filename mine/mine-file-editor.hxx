#pragma once

#include <string>
#include <utility>
#include <optional>
#include <functional>

#include <mine/mine-io-file.hxx>
#include <mine/mine-workspace.hxx>
#include <mine/mine-async-loop.hxx>

namespace mine
{
  // The editor state integrated with file I/O capabilities.
  //
  // The general idea here is to wrap the core workspace (which only knows
  // about text) with the file_document (which knows about disk persistence).
  //
  // Note that I/O operations are pure: they return a new state and a
  // side-effect (effect) to be executed by the caller (usually on an async
  // loop)
  //
  class file_editor
  {
  public:
    file_editor () = default;

    file_editor (file_document b, workspace s)
      : file_doc_ (std::move (b)),
        editor_state_ (std::move (s))
    {
    }

    // Accessors.
    //
    const file_document&
    document () const noexcept { return file_doc_; }

    const workspace&
    editor_workspace () const noexcept { return editor_state_; }

    const std::optional<std::string>&
    message () const noexcept { return message_; }

    // State queries.
    //
    // Note that we consider the "whole" dirty if either the file buffer
    // tracks unsaved changes or the editor state has been modified since
    // the last sync.
    //
    bool
    is_dirty () const noexcept
    {
      return file_doc_.is_dirty () || editor_state_.modified ();
    }

    bool
    io_in_progress () const noexcept
    {
      return file_doc_.io_in_progress ();
    }

    std::optional<std::string>
    file_name () const noexcept
    {
      return file_doc_.file_name ();
    }

    std::optional<float>
    progress_percent () const noexcept
    {
      return file_doc_.progress_percent ();
    }

    // File operations.
    //
    // These return a pair of {new_state, effect}. The caller is responsible
    // for scheduling the effect.
    //

    // Initiate an asynchronous file load.
    //
    // We reset the editor state to match the (potentially empty or partial)
    // new buffer returned by load_file(). As the effect executes, it will
    // dispatch actions to update the content.
    //
    [[nodiscard]] std::pair<file_editor, file_io_effect>
    load (const std::string& fn) const;

    // Initiate a file save.
    //
    [[nodiscard]] std::pair<file_editor, file_io_effect>
    save () const;

    // React to a completed (or progressing) file I/O action.
    //
    [[nodiscard]] file_editor
    update_with_action (file_io_action a) const;

    // React to user editing operations.
    //
    [[nodiscard]] file_editor
    update_with_state (workspace ns) const;

  private:
    file_document file_doc_;
    workspace editor_state_;
    std::optional<std::string> message_;
  };

  // Helper to execute file I/O effect on an async loop.
  //
  // See common/dispatch for details on how the loop handles the lambda.
  //
  inline void
  execute_file_effect (async_loop& l,
                       file_io_effect e,
                       std::function<void (file_io_action)> on_action)
  {
    l.post ([e = std::move (e), oa = std::move (on_action)] ()
    {
      // The effect itself might spawn threads or do blocking work, so ensuring
      // this runs off the UI thread is the caller's job (via the loop
      // implementation).
      //
      e (oa);
    });
  }
}
