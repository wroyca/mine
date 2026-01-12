#pragma once

#include <functional>
#include <optional>
#include <variant>

#include <mine/mine-async-loop.hxx>
#include <mine/mine-core-state.hxx>
#include <mine/mine-io-file.hxx>
#include <mine/mine-command-base.hxx>
#include <mine/mine-terminal-input.hxx>

namespace mine
{
  // Rendering optimization hints.
  //
  // When the state changes, the renderer needs to know "how much" changed.
  // If we just moved the cursor, we don't want to re-tokenize syntax
  // highlighting or redraw the whole screen.
  //
  enum class change_hint
  {
    cursor,  // Cheap: move cursor, maybe update status line.
    content, // Expensive: buffer modified, full re-render.
    view     // Medium: scroll, mostly blit but some new line rendering.
  };

  // Alias for convenience.
  //
  using state_change_type = change_hint;

  // The Nexus.
  //
  class core
  {
  public:
    using change_callback = std::function<void (const state&, change_hint)>;
    using msg_callback = std::function<void (const std::string&)>;

    // We really need the loop to do anything useful (loading/saving).
    //
    explicit
    core (async_loop& l, state s = state ())
      : h_ (std::move (s)),
        l_ (&l)
    {
    }

    // State Access.
    //

    const state&
    current () const noexcept
    {
      return h_.current ();
    }

    // Command Dispatch.
    //

    void
    dispatch (const command& cmd)
    {
      const auto& pre (h_.current ());
      auto post (cmd.execute (pre));

      // Classify the change to help the renderer and history.
      //
      change_hint hint;

      if (cmd.modifies_buffer ())
      {
        hint = change_hint::content;
        h_ = h_.push (std::move (post));
      }
      else if (pre.view () != post.view ())
      {
        // View changes (scrolling) are significant enough to warrant
        // a history entry? debatable. For now, yes, to allow jumping back.
        //
        hint = change_hint::view;
        h_ = h_.push (std::move (post));
      }
      else
      {
        // Cursor movement. Don't push to history stack (spam), just
        // update the "tip" of the current node.
        //
        hint = change_hint::cursor;
        h_ = h_.replace_current (std::move (post));
      }

      notify (hint);
    }

    void
    handle_input (const input_event& e)
    {
      auto cmd (make_command (e));

      if (cmd)
        dispatch (*cmd);
    }

    // History.
    //

    bool
    can_undo () const noexcept { return h_.can_undo (); }

    bool
    can_redo () const noexcept { return h_.can_redo (); }

    void
    undo ()
    {
      if (can_undo ())
      {
        h_ = h_.undo ();
        notify (change_hint::content);
      }
    }

    void
    redo ()
    {
      if (can_redo ())
      {
        h_ = h_.redo ();
        notify (change_hint::content);
      }
    }

    // File I/O.
    //
    // This uses the "Side Effect" pattern. The IO functions return a pure
    // tuple of [new_state, effect_closure]. We apply the state immediately,
    // then schedule the closure on the async loop.
    //

    void
    load (const std::string& path)
    {
      if (!l_) return;

      auto [nfb, eff] = mine::load_file (fb_, path);
      fb_ = std::move (nfb);

      // Reset history when loading a new file.
      //
      auto s (h_.current ()
              .with_buffer (fb_.content)
              .with_modified (false));

      h_ = history (std::move (s));

      run_io (std::move (eff));
      notify (change_hint::content);
    }

    void
    save ()
    {
      if (!l_) return;

      // Can't save a "new file" that hasn't been named yet.
      //
      if (!std::holds_alternative<existing_file> (fb_.state))
        return; // TODO: Trigger "save as" prompt logic here.

      // Sync the editor content into the file buffer before saving.
      //
      fb_.content = h_.current ().buffer ();

      auto [nfb, eff] = mine::save_file (fb_);
      fb_ = std::move (nfb);

      run_io (std::move (eff));
      notify (change_hint::content); // To clear dirty flag visuals.
    }

    // Queries & Callbacks.
    //

    bool
    dirty () const noexcept { return fb_.is_dirty (); }

    bool
    io_busy () const noexcept { return fb_.io_in_progress (); }

    std::optional<std::string>
    filename () const noexcept { return fb_.file_name (); }

    std::optional<float>
    progress () const noexcept { return fb_.progress_percent (); }

    void
    on_change (change_callback c)
    {
      cb_change_ = std::move (c);
    }

    void
    on_message (msg_callback c)
    {
      cb_msg_ = std::move (c);
    }

    // Handle terminal window resize.
    //
    // We need to update the view geometry so that scrolling calculations
    // remain correct.
    //
    // Note that we use `replace_current` here. Resizing is a "meta" change
    // to the editor state, not a semantic edit to the text. We don't want
    // the user to have to hit Undo to revert a window resize.
    //
    void
    resize (screen_size s)
    {
      auto v (h_.current ().view ().resize (s));
      auto ns (h_.current ().with_view (std::move (v)));

      h_ = h_.replace_current (std::move (ns));
      notify (change_hint::view);
    }

  private:
    void
    notify (change_hint h)
    {
      if (cb_change_)
        cb_change_ (h_.current (), h);
    }

    // Run the IO effect on the async loop.
    //
    void
    run_io (io_effect eff)
    {
      // We capture 'this' safely because 'core' outlives the IO operations
      // in the application main loop.
      //
      l_->post ([this, e = std::move (eff)] () mutable
      {
        e ([this] (file_io_action a)
        {
          this->complete_io (std::move (a));
        });
      });
    }

    // Callback from the IO thread (back on the main thread via post).
    //
    void
    complete_io (file_io_action a)
    {
      auto [nfb, msg] = update_file_buffer (fb_, a);
      fb_ = std::move (nfb);

      // If the IO operation resulted in new content (e.g., load finished),
      // update the editor state.
      //
      if (fb_.content != h_.current ().buffer ())
      {
        auto s (h_.current ()
                .with_buffer (fb_.content)
                .with_modified (false));

        // Note: For a reload, we might want to preserve history?
        // For now, treat it as a fresh start.
        //
        h_ = history (std::move (s));
        notify (change_hint::content);
      }

      if (msg && cb_msg_)
        cb_msg_ (*msg);
    }

  private:
    history h_;
    file_buffer fb_;
    async_loop* l_;

    change_callback cb_change_;
    msg_callback cb_msg_;
  };

  // Alias for convenience.
  //
  using editor_core = core;
}
