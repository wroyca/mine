#pragma once

#include <algorithm> // std::min
#include <optional>

#include <mine/mine-types.hxx>
#include <mine/mine-core-buffer.hxx>
#include <mine/mine-core-cursor.hxx>

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
  class view
  {
  public:
    view () = default;

    view (line_number t, screen_size s)
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
    screen_to_buffer (screen_position p, const text_buffer& b) const noexcept;

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
    view
    scroll_to_cursor (const cursor& c, const text_buffer& b) const noexcept
    {
      line_number l (c.line ());
      line_number nt (top_); // new top
      std::size_t h (height ());

      // Case 1: Cursor is above the viewport.
      //
      if (l.value < top_.value)
      {
        nt = l;
      }
      // Case 2: Cursor is below the viewport.
      //
      // Example: If height is 24 and cursor is at line 30, we want line 30
      // to be the last visible line. So the top must be 30 - 24 + 1 = 7.
      // Range: [7...30] (24 lines).
      //
      else if (l.value >= top_.value + h)
      {
        nt = line_number (l.value - h + 1);
      }

      // Clamping.
      //
      // We generally want to avoid scrolling past the end of the file (which
      // would leave the bottom half of the screen blank), unless the file
      // itself is shorter than the screen height.
      //
      std::size_t max_top (0);

      if (b.line_count () > h)
        max_top = b.line_count () - h;

      // Apply the clamp.
      //
      nt = line_number (std::min (nt.value, max_top));

      return view (nt, sz_);
    }

    // Manual scrolling (e.g., PgUp/PgDn/Arrows).
    //
    view
    scroll_up (std::size_t n, const text_buffer& /*b*/) const noexcept
    {
      if (top_.value == 0)
        return *this;

      std::size_t t (top_.value > n ? top_.value - n : 0);
      return view (line_number (t), sz_);
    }

    view
    scroll_down (std::size_t n, const text_buffer& b) const noexcept
    {
      std::size_t h (height ());
      std::size_t max_top (0);

      // Determine the lowest valid top line index.
      //
      if (b.line_count () > h)
        max_top = b.line_count () - h;

      std::size_t t (std::min (top_.value + n, max_top));
      return view (line_number (t), sz_);
    }

    // Resize the viewport (e.g., terminal resize event).
    //
    view
    resize (screen_size s) const noexcept
    {
      // We just update the size but keep the top anchor.
      //
      // Note: A smarter implementation might try to adjust 'top' to keep the
      // cursor centered or visible if the resize caused it to fall off, but
      // we leave that to the main loop (which usually calls scroll_to_cursor
      // immediately after a resize anyway).
      //
      return view (top_, s);
    }

    auto
    operator<=> (const view&) const noexcept = default;

  private:
    line_number top_ {0};
    screen_size sz_ {24, 80};
  };
}
