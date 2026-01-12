#pragma once

#include <string>
#include <utility>
#include <optional>
#include <functional>

#include <mine/mine-io-file.hxx>
#include <mine/mine-core-state.hxx>
#include <mine/mine-async-loop.hxx>

namespace mine
{
  // The editor state integrated with file I/O capabilities.
  //
  // The general idea here is to wrap the core editor_state (which only knows
  // about text) with the file_buffer (which knows about disk persistence).
  //
  // Note that I/O operations are pure: they return a new state and a
  // side-effect (effect) to be executed by the caller (usually on an async
  // loop)
  //
  class editor_with_file
  {
  public:
    editor_with_file () = default;

    editor_with_file (file_buffer b, editor_state s)
      : file_buffer_ (std::move (b)),
        editor_state_ (std::move (s))
    {
    }

    // Accessors.
    //
    const file_buffer&
    buffer () const noexcept { return file_buffer_; }

    const editor_state&
    state () const noexcept { return editor_state_; }

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
      return file_buffer_.is_dirty () || editor_state_.is_modified ();
    }

    bool
    io_in_progress () const noexcept
    {
      return file_buffer_.io_in_progress ();
    }

    std::optional<std::string>
    file_name () const noexcept
    {
      return file_buffer_.file_name ();
    }

    std::optional<float>
    progress_percent () const noexcept
    {
      return file_buffer_.progress_percent ();
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
    std::pair<editor_with_file, file_io_effect>
    load (const std::string& fn) const
    {
      auto [nb, eff] = load_file (file_buffer_, fn);

      // We need to sync the core editor state to this new buffer.
      //
      auto ns (editor_state ().with_buffer (nb.content).with_modified (false));

      return {
        editor_with_file (std::move (nb), std::move (ns)),
        std::move (eff)
      };
    }

    // Initiate a file save.
    //
    std::pair<editor_with_file, file_io_effect>
    save () const
    {
      auto [nb, eff] = save_file (file_buffer_);

      // Once saved, the modification flag on the editor state should
      // theoretically be cleared, though the async nature makes this
      // tricky. We assume optimistic success here for the state flag.
      //
      auto ns (editor_state_.with_modified (false));

      return {
        editor_with_file (std::move (nb), std::move (ns)),
        std::move (eff)
      };
    }

    // React to a completed (or progressing) file I/O action.
    //
    // This is the callback target for the effects dispatched by load/save.
    //
    editor_with_file
    update_with_action (file_io_action a) const
    {
      auto [nb, msg] = update_file_buffer (file_buffer_, a);

      // If the file buffer content changed (e.g., a read chunk arrived),
      // we must propagate that to the editor state.
      //
      // TODO: This involves a string copy. If we end up with massive files,
      // we might want to look into shared buffers or a rope implementation.
      //
      auto ns (editor_state_);

      if (nb.content != file_buffer_.content)
        ns = ns.with_buffer (nb.content).with_modified (false);

      auto r (editor_with_file (std::move (nb), std::move (ns)));
      r.message_ = std::move (msg);

      return r;
    }

    // React to user editing operations.
    //
    // This syncs the editor_state back to the file_buffer.
    //
    editor_with_file
    update_with_state (editor_state ns) const
    {
      auto r (*this);

      // Check if the content changed before we move the new state into place.
      //
      if (ns.buffer () != editor_state_.buffer ())
        r.file_buffer_.content = ns.buffer ();

      r.editor_state_ = std::move (ns);
      return r;
    }

  private:
    file_buffer file_buffer_;
    editor_state editor_state_;
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

#endif // MINE_MINE_EDITOR_FILE_HXX
