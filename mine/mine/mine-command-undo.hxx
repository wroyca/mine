#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  class undo_command : public command
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
