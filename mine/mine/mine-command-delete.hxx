#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
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
    modifies_buffer () const noexcept override;
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
    modifies_buffer () const noexcept override;
  };
}
