#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <map>

#include <lua.hpp>

#include <mine/mine-async-loop.hxx>
#include <mine/mine-command.hxx>
#include <mine/mine-core-state.hxx>
#include <mine/mine-io-file.hxx>
#include <mine/mine-terminal-input.hxx>
#include <mine/mine-utility.hxx>
#include <mine/mine-vm-bridge.hxx>
#include <mine/mine-vm.hxx>

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
    cursor,    // Cheap: move hardware cursor, update status line.
    selection, // Medium: redraw text to update highlight, but no text edits.
    content,   // Expensive: buffer modified, full re-render.
    view       // Expensive: scroll, mostly blit but requires full re-render.
  };

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
    core (async_loop& l, state s = state ());

    // State Access
    //

    const state&
    current () const noexcept
    {
      return h_.current ();
    }

    // Bindings API
    //

    void
    bind_key (std::string_view chord, std::string_view action);

    // Command Dispatch
    //

    void
    dispatch (const command& cmd);

    void
    handle_input (const input_event& e);

    // History
    //

    bool
    can_undo () const noexcept
    {
      return h_.can_undo ();
    }

    bool
    can_redo () const noexcept
    {
      return h_.can_redo ();
    }

    void
    undo ();

    void
    redo ();

    void
    quit ();

    // File I/O
    //
    // This uses the "Side Effect" pattern. The IO functions return a pure
    // tuple of [new_state, effect_closure]. We apply the state immediately,
    // then schedule the closure on the async loop.
    //

    void
    load (const std::string& path);

    void
    save ();

    // Queries & Callbacks
    //

    bool
    dirty () const noexcept
    {
      auto it (files_.find (h_.current ().active_buffer_id ()));
      return it != files_.end () && it->second.is_dirty ();
    }

    bool
    io_busy () const noexcept
    {
      auto it (files_.find (h_.current ().active_buffer_id ()));
      return it != files_.end () && it->second.io_in_progress ();
    }

    std::optional<std::string>
    filename () const noexcept
    {
      auto it (files_.find (h_.current ().active_buffer_id ()));
      return it != files_.end () ? it->second.file_name () : std::nullopt;
    }

    std::optional<float>
    progress () const noexcept
    {
      auto it (files_.find (h_.current ().active_buffer_id ()));
      return it != files_.end () ? it->second.progress_percent () : std::nullopt;
    }

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

    // Display a transient message directly onto the command line prompt.
    //
    void
    show_message (const std::string& m);

    // Scripting configuration initialization.
    //
    void
    load_config ();

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
    resize (screen_size s);

  private:
    void
    notify (change_hint h);

    void
    run_io (io_effect eff, buffer_id id);

    void
    complete_io (buffer_id id, const file_io_action& a);

  private:
    history h_;
    std::map<buffer_id, file_buffer> files_;

    async_loop* l_;
    vm vm_ {vm_limits::permissive()};
    std::map<input_event, std::string> keymaps_;
    std::function<void (std::string_view)> print_handler_;

    change_callback cb_change_;
    msg_callback cb_msg_;
  };

  using editor_core = core;
}
