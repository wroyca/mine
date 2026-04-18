#pragma once

#include <algorithm> // std::min
#include <optional>

#include <mine/mine-types.hxx>
#include <mine/mine-content.hxx>
#include <mine/mine-cursor.hxx>

namespace mine
{
  // The Viewport.
  //
  // This class defines the "window" through which the user looks at the
  // buffer. It maps the infinite vertical space of the buffer (line numbers)
  // to the fixed physical rows of the terminal screen.
  //
  // Like the rest of the core, this is immutable. A scroll operation doesn't
  // change the view in-place; it returns a new view instance.
  //
  class viewport
  {
  public:
    viewport () = default;

    viewport (line_number t, screen_size s)
      : top_ (t),
        sz_ (s)
    {
    }

    // Accessors
    //

    line_number
    top () const noexcept
    {
      return top_;
    }

    screen_size
    size () const noexcept
    {
      return sz_;
    }

    std::size_t
    height () const noexcept
    {
      // Reserve one row at the bottom of the window for its status line.
      // The global cmdline is handled by the layout manager independently
      // at the very bottom of the screen.
      //
      return sz_.rows > 1 ? sz_.rows - 1 : 0;
    }

    // Coordinate Mapping
    //

    bool
    contains (line_number l) const noexcept
    {
      std::size_t h (height ());
      return l.value >= top_.value && l.value < top_.value + h;
    }

    // Map an absolute buffer line index to a relative screen row (0-based).
    //
    // Returns nullopt if the line is currently scrolled out of the viewport.
    //
    std::optional<std::uint16_t>
    screen_row (line_number l) const noexcept
    {
      if (!contains (l))
        return std::nullopt;

      return static_cast<std::uint16_t> (l.value - top_.value);
    }

    // Map a physical terminal coordinate back to a logical buffer coordinate.
    //
    // Since the view owns the vertical scroll offset (top_), it is the natural
    // authority for translating physical screen clicks into logical document
    // positions.
    //
    cursor_position
    screen_to_buffer (screen_position p, const content& b) const noexcept;

    // Scrolling Logic
    //

    // Adjust the viewport so that the given cursor becomes visible.
    //
    // This implements the standard "minimal movement" strategy used by most
    // editors:
    //
    // 1. If the cursor is already visible, don't move.
    // 2. If the cursor is above the top edge, pull the top edge up to the
    //    cursor.
    // 3. If the cursor is below the bottom edge, pull the bottom edge down
    //    to the cursor.
    //
    [[nodiscard]] viewport
    scroll_to_cursor (const cursor& c, const content& b) const noexcept;

    // Manual scrolling (e.g., PgUp/PgDn/Arrows).
    //
    [[nodiscard]] viewport
    scroll_up (std::size_t n, const content& b) const noexcept;

    [[nodiscard]] viewport
    scroll_down (std::size_t n, const content& b) const noexcept;

    // Resize the viewport (e.g., terminal resize event).
    //
    [[nodiscard]] viewport
    resize (screen_size s) const noexcept;

    auto
    operator<=> (const viewport&) const noexcept = default;

  private:
    line_number top_ {0};
    screen_size sz_ {24, 80};
  };
}
