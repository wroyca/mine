#pragma once

#include <vector>
#include <string>
#include <compare>
#include <algorithm>

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>

namespace mine
{
  // Visual attributes for a single cell.
  //
  // We keep this structure relatively compact because we are going to store
  // thousands of these. Note that while modern terminals support 24-bit
  // TrueColor, we currently stick to the standard 8-bit palette (256 colors)
  // to minimize memory bandwidth usage during the diff pass.
  //
  struct cell_attributes
  {
    std::uint8_t fg = 7; // ANSI 37 (White)
    std::uint8_t bg = 0; // ANSI 40 (Black)

    bool bold      = false;
    bool italic    = false;
    bool underline = false;

    bool operator== (const cell_attributes&) const = default;
  };

  // The atomic unit of the screen grid.
  //
  // A "cell" here isn't just a `char`. It holds a complete "Grapheme Cluster"
  // because in the modern world, a single user-perceived character can be a
  // sequence of multiple bytes (UTF-8) or even multiple codepoints (like an
  // Emoji with a skin-tone modifier).
  //
  // Wide characters (CJK, Emoji) present a layout challenge. They visually
  // occupy two columns but strictly speaking belong to a single logical
  // position. We handle this by storing the data in the left cell and marking
  // the right cell as a "continuation". The renderer knows to skip these
  // continuations.
  //
  struct terminal_cell
  {
    std::string     text {" "};
    cell_attributes attrs;
    bool            wide_continuation {false};

    bool operator== (const terminal_cell&) const = default;
  };

  // The in-memory frame buffer.
  //
  // We use this for Double Buffering. The renderer maintains two instances:
  // `current` (what the user sees right now) and `next` (what we want to show
  // them in the next frame).
  //
  // Implementation-wise, we flatten the 2D grid into a single 1D vector. While
  // `vector<vector<cell>>` might seem more natural for a grid, it kills cache
  // locality.
  //
  class terminal_screen
  {
  public:
    terminal_screen () = default;

    explicit
    terminal_screen (screen_size s)
      : size_ (s),
        cells_ (s.rows * s.cols, terminal_cell {})
    {
    }

    screen_size
    size () const noexcept
    {
      return size_;
    }

    // Access
    //

    terminal_cell&
    at (screen_position p)
    {
      MINE_PRECONDITION (size_.contains (p));
      return cells_[p.row * size_.cols + p.col];
    }

    const terminal_cell&
    at (screen_position p) const
    {
      MINE_PRECONDITION (size_.contains (p));
      return cells_[p.row * size_.cols + p.col];
    }

    // Modification
    //

    void
    set_cell (screen_position p, const terminal_cell &c)
    {
      at (p) = std::move (c);
    }

    // Convenience helper for writing simple ASCII.
    //
    void
    set_char (screen_position p, char c, cell_attributes a = {})
    {
      at (p) = terminal_cell {std::string (1, c), a, false};
    }

    // Write a full grapheme cluster.
    //
    // If the grapheme is "wide" (like a Kanji or a Smiley), it will clobber two
    // cells. We write the actual content into `p` and then write a dummy marker
    // into `p + 1`.
    //
    // Note that we check bounds for the continuation cell: if a wide char is
    // written to the very last column, we just clip the continuation (the
    // terminal will likely wrap or clip anyway).
    //
    void
    set_grapheme (screen_position p,
                  std::string_view s,
                  cell_attributes a = {},
                  bool wide = false)
    {
      at (p) = terminal_cell {std::string (s), a, false};

      if (wide && p.col + 1 < size_.cols)
      {
        screen_position next (p.row, p.col + 1);
        at (next) = terminal_cell {"", a, true};
      }
    }

    // Bulk Ops
    //

    void
    clear ()
    {
      std::ranges::fill (cells_, terminal_cell {});
    }

    void
    clear_line (std::uint16_t row)
    {
      MINE_PRECONDITION (row < size_.rows);

      // We can just iterate the segment of the flat vector corresponding to
      // this row.
      //
      auto b (cells_.begin () + (row * size_.cols));
      std::fill_n (b, size_.cols, terminal_cell {});
    }

    // Resize the canvas, preserving content.
    //
    // When the terminal window is resized, we want to try and keep the
    // current content "anchored" at top-left, rather than clearing everything.
    //
    // We compute the intersection of the old rect and the new rect. Content
    // inside the intersection is copied over; content outside is dropped; new
    // space is zero-initialized.
    //
    terminal_screen
    resize (screen_size new_s) const
    {
      terminal_screen r (new_s);

      std::uint16_t h (std::min (size_.rows, new_s.rows));
      std::uint16_t w (std::min (size_.cols, new_s.cols));

      for (std::uint16_t y (0); y < h; ++y)
      {
        for (std::uint16_t x (0); x < w; ++x)
        {
          screen_position p (y, x);
          r.set_cell (p, at (p));
        }
      }

      return r;
    }

    bool operator== (const terminal_screen&) const = default;

  private:
    screen_size size_ {24, 80};
    std::vector<terminal_cell> cells_;
  };

  // Diffing
  //

  // A list of point-mutations required to transition one frame to another.
  //
  struct screen_diff
  {
    struct change
    {
      screen_position pos;
      terminal_cell   cell;
    };

    std::vector<change> changes;

    bool
    empty () const noexcept
    {
      return changes.empty ();
    }
  };

  // Compute the minimal set of updates.
  //
  // This is the core optimization of the renderer. Writing to a TTY is
  // surprisingly expensive (syscall overhead, kernel buffering, potentially
  // network latency if over SSH).
  //
  // By comparing the previous frame (`old_s`) to the next frame (`new_s`) in
  // memory, we can emit ANSI codes *only* for the cells that actually
  // changed.
  //
  // Note that this requires both screens to have the same dimensions. If the
  // user resized the window, the whole coordinate system shifted, so a diff
  // is meaningless (and we should force a full redraw instead).
  //
  inline screen_diff
  compute_screen_diff (const terminal_screen& old_s,
                       const terminal_screen& new_s,
                       std::uint16_t row_start = 0,
                       std::uint16_t row_count = 0)
  {
    MINE_PRECONDITION (old_s.size () == new_s.size ());

    screen_diff d;
    screen_size sz (new_s.size ());

    // If count is 0 (default), go to the end.
    //
    if (row_count == 0)
      row_count = sz.rows - row_start;

    // Sanity check the bounds.
    //
    std::uint16_t row_end (std::min<uint16_t> (row_start + row_count, sz.rows));

    // Linear scan.
    //
    // Because we flattened the vector in `terminal_screen`, this loop walks
    // contiguous memory. (CPU prefetcher will love this ;) ).
    //
    for (std::uint16_t r (row_start); r < row_end; ++r)
    {
      for (std::uint16_t c (0); c < sz.cols; ++c)
      {
        screen_position p (r, c);

        // We rely on `terminal_cell::operator==` here. If the content, color,
        // or attributes are different, we record a change.
        //
        if (old_s.at (p) != new_s.at (p))
          d.changes.push_back ({p, new_s.at (p)});
      }
    }

    return d;
  }
}
