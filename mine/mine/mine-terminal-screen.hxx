#pragma once

#include <algorithm>
#include <compare>
#include <vector>

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>

namespace mine
{
  // Visual properties of a single character cell.
  //
  // We stick to the standard ANSI attributes here. While modern terminals
  // support RGB (TrueColor), we start with the basic 8-bit palette (256 colors)
  // for broad compatibility and smaller memory footprint.
  //
  struct cell_attributes
  {
    std::uint8_t fg_color = 7;  // Default: White (ANSI 37)
    std::uint8_t bg_color = 0;  // Default: Black (ANSI 40)

    bool bold      = false;
    bool italic    = false;
    bool underline = false;

    bool operator== (const cell_attributes&) const = default;
  };

  // The atomic unit of the screen grid.
  //
  struct terminal_cell
  {
    char ch = ' ';
    cell_attributes attrs;

    bool operator== (const terminal_cell&) const = default;
  };

  // A memory representation of the terminal state.
  //
  // We use this for double-buffering. The renderer keeps the "current"
  // state (what's on screen) and builds the "next" state.
  //
  // We flatten the 2D grid into a 1D vector to keep the prefetcher happy
  // during linear scans (which happen constantly during diffing).
  //
  class terminal_screen
  {
  public:
    terminal_screen () = default;

    explicit terminal_screen (screen_size s)
        : size_ (s),
          cells_ (s.rows * s.cols, terminal_cell {})
    {
    }

    screen_size
    size () const noexcept { return size_; }

    // Accessors.
    //
    terminal_cell&
    at (screen_position pos)
    {
      MINE_PRECONDITION (size_.contains (pos));
      return cells_[pos.row * size_.cols + pos.col];
    }

    const terminal_cell&
    at (screen_position pos) const
    {
      MINE_PRECONDITION (size_.contains (pos));
      return cells_[pos.row * size_.cols + pos.col];
    }

    // Modifiers.
    //
    void
    set_cell (screen_position pos, terminal_cell c)
    {
      at (pos) = c;
    }

    void
    set_char (screen_position pos, char ch, cell_attributes attrs = {})
    {
      at (pos) = terminal_cell {ch, attrs};
    }

    // Bulk operations.
    //
    void
    clear ()
    {
      std::fill (cells_.begin (), cells_.end (), terminal_cell {});
    }

    void
    clear_line (std::uint16_t row)
    {
      MINE_PRECONDITION (row < size_.rows);

      auto start (cells_.begin () + (row * size_.cols));
      std::fill (start, start + size_.cols, terminal_cell {});
    }

    // Create a new screen with different dimensions.
    //
    // We attempt to preserve the content of the top-left corner (0,0) up
    // to the bounds of the new size (clipping if smaller, padding if larger).
    //
    terminal_screen
    resize (screen_size new_size) const
    {
      terminal_screen r (new_size);

      std::uint16_t rows (std::min (size_.rows, new_size.rows));
      std::uint16_t cols (std::min (size_.cols, new_size.cols));

      for (std::uint16_t y (0); y < rows; ++y)
      {
        for (std::uint16_t x (0); x < cols; ++x)
        {
          screen_position pos (y, x);
          r.set_cell (pos, at (pos));
        }
      }

      return r;
    }

    bool operator== (const terminal_screen&) const = default;

  private:
    screen_size size_ {24, 80};
    std::vector<terminal_cell> cells_;
  };

  // The delta between two frames.
  //
  // Used to minimize I/O bandwidth. Instead of redrawing the whole screen
  // (which is slow over SSH), we only emit ANSI codes for cells that changed.
  //
  struct screen_diff
  {
    struct change
    {
      screen_position pos;
      terminal_cell cell;
    };

    std::vector<change> changes;

    bool
    empty () const noexcept { return changes.empty (); }
  };

  // Calculate the difference.
  //
  // Note: This requires both screens to have the same dimensions. If the
  // terminal resized, the diff is undefined (the caller should force a
  // full redraw).
  //
  inline screen_diff
  compute_screen_diff (const terminal_screen& old_s,
                       const terminal_screen& new_s)
  {
    MINE_PRECONDITION (old_s.size () == new_s.size ());

    screen_diff d;
    screen_size sz (new_s.size ());

    for (std::uint16_t r (0); r < sz.rows; ++r)
    {
      for (std::uint16_t c (0); c < sz.cols; ++c)
      {
        screen_position pos (r, c);

        // Optimization: checking inequality is cheap, allocations are not.
        //
        if (old_s.at (pos) != new_s.at (pos))
          d.changes.push_back ({pos, new_s.at (pos)});
      }
    }

    return d;
  }
}
