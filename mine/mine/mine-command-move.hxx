#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
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
    modifies_buffer () const noexcept override;

  private:
    move_direction d_;
    bool select_;
  };

#pragma pack(pop)
}
