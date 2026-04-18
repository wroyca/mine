#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <map>

#include <lua.hpp>

#include <mine/mine-command.hxx>
#include <mine/mine-workspace.hxx>
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

  using change_hint_type = change_hint;

  // The Nexus.
  //
  class editor
  {
  public:
    using change_callback = std::function<void (const workspace&, change_hint)>;
    using msg_callback = std::function<void (const std::string&)>;
    using save_callback = std::function<void (document_id, std::string, content)>;

    explicit
    editor (workspace s = workspace ());

    // State Access
    //

    const workspace&
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

    // Document lifecycle.
    //
    // These are called by the application layer after async I/O completes.
    // The editor itself never initiates I/O.
    //
    void
    open_document (const std::string& path, content text);

    void
    mark_saved (document_id id);

    // Queries & Callbacks
    //

    bool
    quit_requested () const noexcept
    {
      return quit_requested_;
    }

    bool
    dirty () const noexcept
    {
      return h_.current ().modified ();
    }

    std::optional<std::string>
    filename () const noexcept
    {
      auto const& doc (h_.current ().get_document (
        h_.current ().active_document_id ()));

      if (doc.name.empty ())
        return std::nullopt;

      return doc.name;
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

    void
    on_save (save_callback c)
    {
      cb_save_ = std::move (c);
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
    save ();



  private:
    edit_history h_;
    bool quit_requested_ {false};

    vm vm_ {vm_limits::permissive()};
    std::map<input_event, std::string> keymaps_;
    std::function<void (std::string_view)> print_handler_;

    change_callback cb_change_;
    msg_callback cb_msg_;
    save_callback cb_save_;
  };
}
