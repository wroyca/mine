#pragma once

#include <compare>
#include <optional>

#include <mine/mine-types.hxx>
#include <mine/mine-content.hxx>

#include <mine/mine-contract.hxx>

namespace mine
{
  // A cursor in grapheme cluster space.
  //
  // This class represents the "point" in the editor. Note that all coordinates
  // here are in terms of *grapheme clusters*, not bytes or code points.
  //
  // If the user presses "Right" while standing on a flag emoji, we increment
  // the column by 1, which might correspond to jumping 10+ bytes in the buffer.
  //
  // In other words, we rely on the buffer to define what "column N" actually
  // means in memory.
  //
  class cursor
  {
  public:
    cursor () = default;

    explicit
    cursor (cursor_position p)
      : p_ (p)
    {
    }

    // Accessors
    //

    cursor_position
    position () const noexcept
    {
      return p_;
    }

    line_number
    line () const noexcept
    {
      return p_.line;
    }

    column_number
    column () const noexcept
    {
      return p_.column;
    }

    // Selection mark
    //

    void
    set_mark () noexcept
    {
      m_ = p_;
    }

    void
    clear_mark () noexcept
    {
      m_.reset ();
    }

    bool
    has_mark () const noexcept
    {
      return m_.has_value ();
    }

    cursor_position
    mark () const noexcept
    {
      MINE_PRECONDITION (has_mark ());

      return *m_;
    }

    // Navigation
    //

    [[nodiscard]] cursor
    move_to (cursor_position p) const noexcept;

    [[nodiscard]] cursor
    move_left (const content& b) const noexcept;

    [[nodiscard]] cursor
    move_right (const content& b) const noexcept;

    [[nodiscard]] cursor
    move_up (const content& b) const noexcept;

    [[nodiscard]] cursor
    move_down (const content& b) const noexcept;

    // Semantic Jumps
    //

    [[nodiscard]] cursor
    move_line_start () const noexcept;

    [[nodiscard]] cursor
    move_line_end (const content& b) const noexcept;

    [[nodiscard]] cursor
    move_buffer_start () const noexcept;

    [[nodiscard]] cursor
    move_buffer_end (const content& b) const noexcept;

    // Validation
    //

    [[nodiscard]] cursor
    clamp_to_buffer (const content& b) const noexcept;

    auto
    operator<=> (const cursor&) const noexcept = default;

  private:
    cursor_position p_;
    std::optional<cursor_position> m_;
  };
}
