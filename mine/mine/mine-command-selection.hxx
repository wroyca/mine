#pragma once

#include <mine/mine-command-base.hxx>

#include <cstdint>
#include <string_view>

namespace mine
{
  // Handle the initial mouse press. We just move the cursor to the target
  // coordinates and drop the selection anchor (the mark) here.
  //
  class begin_selection_command: public command
  {
  public:
    explicit
    begin_selection_command (std::uint16_t x, std::uint16_t y);

    editor_state
    execute (const editor_state& s) const override;

    std::string_view
    name () const noexcept override;

    bool
    modifies_buffer () const noexcept override;

  private:
    std::uint16_t x_;
    std::uint16_t y_;
  };

  // Fire as the mouse is dragged. We update the cursor point while leaving the
  // original anchor intact, thereby highlighting the region between them.
  //
  class update_selection_command: public command
  {
  public:
    explicit
    update_selection_command (std::uint16_t x, std::uint16_t y);

    editor_state
    execute (const editor_state& s) const override;

    std::string_view
    name () const noexcept override;

    bool
    modifies_buffer () const noexcept override;

  private:
    std::uint16_t x_;
    std::uint16_t y_;
  };

  // Trigger when the mouse button is released. In a local terminal we might
  // just leave the selection active. Though in some environments, this is the
  // hook we use to copy the highlighted text to the system clipboard.
  //
  class end_selection_command: public command
  {
  public:
    explicit
    end_selection_command (std::uint16_t x, std::uint16_t y);

    editor_state
    execute (const editor_state& s) const override;

    std::string_view
    name () const noexcept override;

    bool
    modifies_buffer () const noexcept override;

  private:
    std::uint16_t x_;
    std::uint16_t y_;
  };
}
