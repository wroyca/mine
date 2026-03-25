#pragma once

#include <string>
#include <utility>

#include <mine/mine-command-base.hxx>
#include <mine/mine-unicode.hxx>

namespace mine
{
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
    modifies_buffer () const noexcept override;

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
    modifies_buffer () const noexcept override;
  };
}
