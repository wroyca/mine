#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <optional>

#include <mine/mine-clipboard.hxx>
#include <mine/mine-core-state.hxx>
#include <mine/mine-terminal-input.hxx>
#include <mine/mine-unicode.hxx>

namespace mine
{
  // The base interface for all editor actions.
  //
  // The model here is purely functional: a command takes the current state and
  // returns a new one. This makes things like undo/redo or replay logs trivial,
  // at the cost of some copying (hopefully mitigated by data sharing in the
  // state object).
  //
  class command
  {
  public:
    virtual ~command () = 0;

    command ()                           = default;
    command (const command&)             = default;
    command (command&&)                  = default;
    command& operator = (const command&) = default;
    command& operator = (command&&)      = default;

    // Apply the command logic.
    //
    // Note that we pass the state by const reference. The implementation must
    // return a modified copy.
    //
    [[nodiscard]] virtual editor_state
    execute (const editor_state& s) const = 0;

    // Diagnostic name (e.g., "insert-char", "move-down").
    //
    [[nodiscard]] virtual std::string_view
    name () const noexcept = 0;

    // Return true if this command mutates the text buffer (as opposed to just
    // moving the cursor or scrolling). We use this to decide whether to push a
    // new undo frame.
    //
    [[nodiscard]] virtual bool
    modifies_buffer (const editor_state& s) const noexcept = 0;
  };

  // Parse a key chord string (like "C-o" or "S-up") into an input event.
  //
  std::optional<input_event>
  parse_key_chord (std::string_view chord);

  // Translate raw input events into semantic commands.
  //
  // Returns nullptr if the event doesn't map to any known command.
  //
  std::unique_ptr<command>
  make_command (const input_event& e);

  // Create a command object by its semantic string name (e.g. "insert_newline").
  //
  std::unique_ptr<command>
  make_command_by_name (std::string_view name);

  // Parse a Vim-style ex command string into a semantic command.
  //
  std::unique_ptr<command>
  parse_cmdline (std::string_view cmd);

  class copy_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  class paste_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  // Handle the Backspace key.
  //
  // Semantically, backspacing is a compound operation: we attempt to move the
  // cursor "back" (left/up) one grapheme cluster, and if successful, we delete
  // the grapheme at that new position.
  //
  class delete_backward_command: public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  // Handle the Delete key.
  //
  // This operation removes the grapheme currently under the cursor (or merges
  // lines if the cursor is at a newline). The cursor position itself remains
  // technically unchanged, though the content shifts "left" to fill the
  // void.
  //
  class delete_forward_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  // Insert a span of text at the current cursor position.
  //
  // Note that this command expects a "flat" string of text (no control
  // characters or newlines). We assume that the input layer has already
  // stripped or processed those, routing <Enter> to insert_newline_command,
  // for example.
  //
  class insert_text_command: public command
  {
  public:
    explicit
    insert_text_command (std::string text);

    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;

  private:
    std::string text_;
  };

  // Insert a logical newline (break the current line).
  //
  // This operation splits the current line at the cursor position and moves
  // the cursor to the beginning (column 0) of the newly created line.
  //
  class insert_newline_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  // The vocabulary of abstract movement directions.
  //
  // Note that we mix cursor movement (up/down/left/right) with view
  // manipulation (scroll) here. While semantically different, they are usually
  // triggered by the same class of input events (navigation keys/mouse wheel).
  //
  enum class move_direction : std::uint8_t
  {
    // Relative cursor movement.
    //
    up,
    down,
    left,
    right,

    // Logical line boundaries.
    //
    line_start,   // Home
    line_end,     // End

    // Buffer boundaries.
    //
    buffer_start, // Ctrl+Home
    buffer_end,   // Ctrl+End

    // Viewport manipulation.
    //
    scroll_up,    // Mouse wheel up
    scroll_down   // Mouse wheel down
  };

  // Execute a navigation or scrolling action.
  //
  // This command is read-only regarding the buffer content (it returns false
  // for modifies_buffer), but it produces a new editor state with either an
  // updated cursor position or an updated viewport offset.
  //
#pragma pack(push, 1)

  class move_cursor_command : public command
  {
  public:
    explicit
    move_cursor_command (move_direction d, bool select = false);

    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;

  private:
    move_direction d_;
    bool select_;
  };

#pragma pack(pop)

  class save_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  class save_and_quit_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  class quit_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  class redo_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

#pragma pack(push, 1)

  // Handle the initial mouse press. We just move the cursor to the target
  // coordinates and drop the selection anchor (the mark) here.
  //
  class begin_selection_command : public command
  {
  public:
    explicit
    begin_selection_command (std::uint16_t x, std::uint16_t y);

    [[nodiscard]] editor_state
    execute (const editor_state &s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;

  private:
    std::uint16_t x_;
    std::uint16_t y_;
  };

  // Fire as the mouse is dragged. We update the cursor point while leaving the
  // original anchor intact, thereby highlighting the region between them.
  //
  class update_selection_command : public command
  {
  public:
    explicit
    update_selection_command (std::uint16_t x, std::uint16_t y);

    [[nodiscard]] editor_state
    execute (const editor_state &s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;

  private:
    std::uint16_t x_;
    std::uint16_t y_;
  };

  // Trigger when the mouse button is released. In a local terminal we might
  // just leave the selection active. Though in some environments, this is the
  // hook we use to copy the highlighted text to the system clipboard.
  //
  class end_selection_command : public command
  {
  public:
    explicit
    end_selection_command (std::uint16_t x, std::uint16_t y);

    [[nodiscard]] editor_state
    execute (const editor_state &s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;

  private:
    std::uint16_t x_;
    std::uint16_t y_;
  };

#pragma pack(pop)

  class undo_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  class toggle_cmdline_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };

  class escape_command : public command
  {
  public:
    [[nodiscard]] editor_state
    execute (const editor_state& s) const override;

    [[nodiscard]] std::string_view
    name () const noexcept override;

    [[nodiscard]] bool
    modifies_buffer (const editor_state& s) const noexcept override;
  };
}
