#pragma once

#include <algorithm> // min, max
#include <optional>

#include <mine/mine-types.hxx>
#include <mine/mine-core-buffer.hxx>
#include <mine/mine-core-cursor.hxx>

namespace mine
{
  // The Viewport.
  //
  // This defines the window through which we see the buffer. It maps "buffer
  // space" (infinite vertical scroll) to "screen space" (fixed rows).
  //
  // Like everything else in the core, this is immutable. Scrolling returns
  // a new view.
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

    // Accessors.
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
      // Reserve one row for the status line.
      //
      // @@: Hardcoding this is obviously the wrong approach, but
      // it will do until we implement proper view composition.
      //
      return sz_.rows > 0 ? sz_.rows - 1 : 0;
    }

    // Hit testing / Coordinate mapping.
    //

    bool
    contains (line_number l) const noexcept
    {
      return l.value >= top_.value &&
             l.value < top_.value + height ();
    }

    // Map a buffer line index to a screen row (0-based relative to the view).
    //
    // Returns nullopt if the line is currently scrolled out of view.
    //
    std::optional<std::uint16_t>
    screen_row (line_number l) const noexcept
    {
      if (!contains (l))
        return std::nullopt;

      return static_cast<std::uint16_t> (l.value - top_.value);
    }

    // Scrolling.
    //

    // Adjust the viewport so that the cursor `c` becomes visible.
    //
    // The logic here is "minimal movement":
    //
    // 1. If the cursor is above the top, move top up to the cursor.
    // 2. If the cursor is below the bottom, move top down just enough to
    //    show it.
    // 3. Finally, clamp everything so we don't show empty space past EOF
    //    unless the file is actually shorter than the screen.
    //
    view
    scroll_to_cursor (const cursor& c, const text_buffer& b) const noexcept
    {
      line_number l (c.line ());
      line_number new_top (top_);

      // Cursor is above us?
      //
      if (l.value < top_.value)
        new_top = l;

      // Cursor is below us?
      //
      else if (l.value >= top_.value + height ())
      {
        // Example: Height 24. Cursor at 30.
        //
        // We want 30 to be the last visible line.
        //
        // New top = 30 - 24 + 1 = 7.
        // Visible: 7..30 (24 lines).
        //
        new_top = line_number (l.value - height () + 1);
      }

      // Clamp.
      //
      // We generally don't want to scroll past the end of the file (leaving
      // half the screen blank) if we have enough content to fill the screen.
      //
      std::size_t max_top (0);
      if (b.line_count () > height ())
        max_top = b.line_count () - height ();

      new_top = line_number (std::min (new_top.value, max_top));

      return view (new_top, sz_);
    }

    // Manual scrolling (PageUp/Down, arrow keys).
    //
    view
    scroll_up (std::size_t n, const text_buffer& b) const noexcept
    {
      if (top_.value == 0)
        return *this;

      std::size_t t (top_.value > n ? top_.value - n : 0);
      return view (line_number (t), sz_);
    }

    view
    scroll_down (std::size_t n, const text_buffer& b) const noexcept
    {
      // Calculate the maximum valid top line.
      //
      std::size_t max_top (0);
      if (b.line_count () > height ())
        max_top = b.line_count () - height ();

      std::size_t t (std::min (top_.value + n, max_top));
      return view (line_number (t), sz_);
    }

    view
    resize (screen_size s) const noexcept
    {
      return view (top_, s);
    }

    auto
    operator<=> (const view&) const noexcept = default;

  private:
    line_number top_ {0};
    screen_size sz_ {24, 80};
  };
}
