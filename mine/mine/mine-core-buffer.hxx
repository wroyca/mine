#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <utility> // move

#include <immer/vector.hpp>
#include <immer/flex_vector.hpp>

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>

namespace mine
{
  // The persistent text buffer core.
  //
  // Unlike traditional editors (like Emacs) that rely on gap buffers or ropes
  // (which are inherently mutable), we use persistent data structures via the
  // `immer` library.
  //
  // The data model is a "vector of vectors":
  //
  //   flex_vector< flex_vector<char> >
  //
  // While a gap buffer offers O(1) insertion at the cursor, it makes
  // implementing reliable undo/redo and asynchronous rendering quite painful.
  //
  // Instead we use persistent vectors (RB-tree based), which trade some raw
  // single-thread throughput for architectural simplicity:
  //
  // 1. Structural Sharing: Modifying a line creates a new path in the RB-tree,
  //    sharing the vast majority of memory with the previous version. We rarely
  //    copy actual text.
  //
  // 2. Trivial Undo/Redo: The undo stack is just `vector<text_buffer>`. We
  //    store snapshots, not reverse-deltas.
  //
  // 3. Lock-Free Reads: The rendering thread can read version N of the buffer
  //    while the input thread is busy computing version N+1.
  //
  class text_buffer
  {
  public:
    // We use `flex_vector` for lines because we need efficient slicing and
    // concatenation capabilities to handle the "Enter" key (splitting lines)
    // and range deletions (merging lines).
    //
    using line_type = immer::flex_vector<char>;
    using lines_type = immer::flex_vector<line_type>;

    // Invariant: The buffer must always contain at least one line.
    //
    // We represent a conceptually "empty file" as a buffer containing exactly
    // one empty line: `[ [] ]`.
    //
    text_buffer ()
        : lines_ ({line_type {}})
    {
    }

    explicit text_buffer (lines_type ls)
        : lines_ (std::move (ls))
    {
      MINE_INVARIANT (!lines_.empty ());
    }

    // Queries.
    //

    std::size_t
    line_count () const noexcept
    {
      return lines_.size ();
    }

    bool
    contains (line_number ln) const noexcept
    {
      return ln.value < lines_.size ();
    }

    const line_type&
    line_at (line_number ln) const
    {
      MINE_PRECONDITION (contains (ln));
      return lines_[ln.value];
    }

    std::size_t
    line_length (line_number ln) const
    {
      MINE_PRECONDITION (contains (ln));
      return lines_[ln.value].size ();
    }

    // Safe character access.
    //
    // Returns nullopt if the coordinates are out of bounds.
    //
    std::optional<char>
    char_at (cursor_position p) const noexcept
    {
      if (!contains (p.line))
        return std::nullopt;

      const auto& l (lines_[p.line.value]);

      if (p.column.value >= l.size ())
        return std::nullopt;

      return l[p.column.value];
    }

    // Mutations.
    //
    // All operations here are pure functions. They return a *new* buffer
    // instance representing the state of the world after the edit. The
    // current instance (`this`) remains untouched (and potentially referenced
    // by the undo stack).
    //

    text_buffer
    insert_char (cursor_position p, char c) const
    {
      MINE_PRECONDITION (contains (p.line));
      MINE_PRECONDITION (p.column.value <= lines_[p.line.value].size ());

      // In a gap buffer, we'd just slide memory. Here, we allocate a new
      // string, mutate it, and then tell immer to update the pointer in the
      // tree.
      //
      return text_buffer (lines_.update (p.line.value,
                                         [&] (auto l)
      {
        return l.insert (p.column.value, c);
      }));
    }

    text_buffer
    insert_string (cursor_position p, std::string_view s) const
    {
      MINE_PRECONDITION (contains (p.line));
      MINE_PRECONDITION (p.column.value <= lines_[p.line.value].size ());

      // It is unfortunate that `immer` doesn't strictly support range insertion
      // on `flex_vector` yet. We have to loop.
      //
      // That said, since this is predominantly driven by human typing speed or
      // relatively small clipboards, the O(M * log N) cost is acceptable for
      // now. If this becomes a bottleneck for massive pastes, we can look into
      // building a transient vector first.
      //
      return text_buffer (lines_.update (p.line.value,
                                         [&] (auto l)
      {
        for (std::size_t i (0); i < s.size (); ++i)
          l = l.insert (p.column.value + i, s[i]);

        return l;
      }));
    }

    // Split the current line at the cursor (The 'Enter' key).
    //
    // Physically, this turns Line N into [Left Part] and inserts [Right Part]
    // at Line N+1.
    //
    text_buffer
    insert_newline (cursor_position p) const
    {
      MINE_PRECONDITION (contains (p.line));

      const auto& l (lines_[p.line.value]);
      MINE_PRECONDITION (p.column.value <= l.size ());

      // Slice the line. `take` and `drop` are O(log L) operations on the
      // line vector.
      //
      line_type lhs (l.take (p.column.value));
      line_type rhs (l.drop (p.column.value));

      // Update the line tree.
      //
      // 1. Replace the current line with just the left part.
      // 2. Insert the right part as a new line immediately after.
      //
      auto r (lines_);
      r = r.set (p.line.value, std::move (lhs));
      r = r.insert (p.line.value + 1, std::move (rhs));

      return text_buffer (std::move (r));
    }

    text_buffer
    delete_char (cursor_position p) const
    {
      MINE_PRECONDITION (contains (p.line));

      if (p.column.value >= lines_[p.line.value].size ())
        return *this;

      return text_buffer (lines_.update (p.line.value,
                                         [&] (auto l)
      {
        return l.erase (p.column.value);
      }));
    }

    // Delete a range of text, potentially spanning multiple lines.
    //
    // The general algorithm ("clip and merge") is:
    //
    // 1. Keep the prefix of the start line [0, start_col).
    // 2. Keep the suffix of the end line [end_col, EOL).
    // 3. Merge them into a single line.
    // 4. Remove all intermediate lines that were fully selected.
    //
    text_buffer
    delete_range (cursor_position b, cursor_position e) const
    {
      MINE_PRECONDITION (b.line.value <= e.line.value);
      MINE_PRECONDITION (b.line.value < lines_.size ());
      MINE_PRECONDITION (e.line.value < lines_.size ());

      // Optimization: If the range is entirely within one line, we don't
      // need to touch the outer vector structure (no nodes added/removed),
      // just update the content of one leaf.
      //
      if (b.line == e.line)
      {
        if (b.column == e.column)
          return *this;

        auto n (e.column.value - b.column.value);
        return text_buffer (lines_.update (b.line.value,
                                           [&] (auto l)
        {
          return l.erase (b.column.value, n);
        }));
      }

      // The multi-line case.
      //
      const auto& lb (lines_[b.line.value]);
      const auto& le (lines_[e.line.value]);

      // Slice out the parts we want to keep.
      //
      line_type pre (lb.take (b.column.value));
      line_type suf (le.drop (e.column.value));

      // Join them. `operator+` on flex_vectors handles the tree rebalancing.
      //
      line_type j (pre + suf);

      // Replace the start line with the joined result.
      //
      auto r (lines_.set (b.line.value, std::move (j)));

      // Remove the lines that were "eaten" by the merge.
      //
      // The number of lines to remove is exactly (end_line - start_line).
      //
      // Example: Start line 10, End line 12.
      //
      // 1. We merged contents of 10 and 12 into 10.
      // 2. We effectively remove line 11 and the *old* line 12.
      //
      auto n (e.line.value - b.line.value);
      r = r.erase (b.line.value + 1, n);

      return text_buffer (std::move (r));
    }

    // Append a fully formed line to the end of the buffer.
    //
    text_buffer
    append_line (line_type l) const
    {
      return text_buffer (lines_.push_back (std::move (l)));
    }

    // Access to the raw container.
    //
    const lines_type&
    lines () const noexcept { return lines_; }

    // Structural equality.
    //
    // Thanks to structural sharing, if two buffers share the same root node,
    // this check is O(1). If they are different but have identical content,
    // it's O(N).
    //
    bool
    operator== (const text_buffer& o) const noexcept
    {
      return lines_ == o.lines_;
    }

  private:
    lines_type lines_;
  };

  // Factories.
  //

  inline text_buffer
  make_empty_buffer ()
  {
    return text_buffer ();
  }

  // Populate a buffer from a raw string.
  //
  // We split on '\n'. Note that we currently assume Unix line endings.
  // CRLF inputs might result in stray '\r' characters at the end of lines,
  // which we don't strictly forbid (they are just characters) but might
  // look ugly in the editor rendering.
  //
  inline text_buffer
  make_buffer_from_string (std::string_view s)
  {
    text_buffer::lines_type ls;

    std::size_t b (0);
    std::size_t e (s.find ('\n'));

    while (e != std::string_view::npos)
    {
      // Construct the flex_vector directly from iterators to avoid
      // manual push_back loop or intermediate std::string allocation.
      //
      auto sub (s.substr (b, e - b));
      text_buffer::line_type l (sub.begin (), sub.end ());

      ls = ls.push_back (std::move (l));
      b = e + 1;
      e = s.find ('\n', b);
    }

    // Handle the tail (the text after the last newline, or the whole
    // string if no newline was found).
    //
    // Example: "A\n" -> Loop handles "A", b points to EOF.
    //          Tail is empty string "".
    //          Result: ["A", ""] (Correct, 2 lines).
    //
    auto sub (s.substr (b));
    text_buffer::line_type l (sub.begin (), sub.end ());

    ls = ls.push_back (std::move (l));

    return text_buffer (std::move (ls));
  }
}
