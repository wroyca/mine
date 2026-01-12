#pragma once

#include <memory>
#include <string_view>

#include <mine/mine-core-state.hxx>
#include <mine/mine-terminal-input.hxx>

namespace mine
{
  // The base interface for all editor actions.
  //
  // The model here is purely functional: a command takes the current state
  // and returns a new one. This makes things like undo/redo or replay logs
  // trivial, at the cost of some copying (hopefully mitigated by data
  // sharing in the state object).
  //
  class command
  {
  public:
    virtual ~command () = default;

    // Apply the command logic.
    //
    // Note that we pass the state by const reference. The implementation
    // must return a modified copy.
    //
    virtual editor_state
    execute (const editor_state& s) const = 0;

    // Diagnostic name (e.g., "insert-char", "move-down").
    //
    virtual std::string_view
    name () const noexcept = 0;

    // Return true if this command mutates the text buffer (as opposed to
    // just moving the cursor or scrolling). We use this to decide whether
    // to push a new undo frame.
    //
    virtual bool
    modifies_buffer () const noexcept = 0;
  };

  using command_ptr = std::unique_ptr<command>;

  // Translate raw input events into semantic commands.
  //
  // Returns nullptr if the event doesn't map to any known command.
  //
  command_ptr
  make_command (const input_event& e);
}
