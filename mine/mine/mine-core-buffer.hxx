#pragma once

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <string_view>

#include <immer/vector.hpp>
#include <immer/flex_vector.hpp>
#include <immer/algorithm.hpp>

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>
#include <mine/mine-unicode-grapheme-index.hxx>

namespace mine
{
  // The core persistent text buffer.
  //
  // We are building this editor around the concept of immutable data
  // structures (using the immer library). This might seem heavy for a simple
  // text buffer, but it gives us structural sharing for free. This means we
  // can keep a history of the entire buffer state for undo/redo without
  // actually copying the text string every time the user types a character.
  //
  // The second architectural pillar here is "Grapheme First". Most C++
  // strings are byte-oriented, or at best code-point oriented. But users
  // see grapheme clusters (e.g., a flag emoji or a character with an accent).
  //
  // If we split a cluster, we break the rendering. So, while we store UTF-8
  // bytes physically, all our logical coordinates (lines and columns) refer
  // to grapheme indices.
  //
  class text_buffer
  {
  public:
    // A single line of text.
    //
    // We wrap the raw storage to maintain a cached grapheme index. We made
    // the index `mutable` because calculating grapheme boundaries is
    // expensive (linear scan of UTF-8), so we want to compute it lazily or
    // update it behind the scenes. It's logically part of the line's "value",
    // but physically it's just a derived cache.
    //
    // We also cache the assembled string because `immer::flex_vector` does not
    // guarantee continuous chunks in memory, and interacting with C-style
    // APIs (like ICU) requires a contiguous block.
    //
    struct line
    {
      immer::flex_vector<char> data;
      mutable std::string      str_cache;
      mutable bool             str_valid {false};
      mutable grapheme_index   idx;

      line () = default;

      explicit
      line (immer::flex_vector<char> d)
          : data (std::move (d))
      {
        // We have to build the index immediately upon construction because
        // the rest of the system assumes line_length() is cheap and valid.
        //
        update_idx ();
      }

      explicit
      line (std::string_view s)
          : data (s.begin (), s.end ()),
            str_cache (s),
            str_valid (true)
      {
        update_idx ();
      }

      line (const line& x)
          : data (x.data)
      {
        // When we copy a line, we can't necessarily trust that the source's
        // index is movable or that we want to share the cache if the underlying
        // implementation changes. Safe bet is to just rebuild it.
        //
        update_idx ();
      }

      line&
      operator= (const line& x)
      {
        if (this != &x)
        {
          data = x.data;
          str_valid = false;
          update_idx ();
        }
        return *this;
      }

      line (line&& x) noexcept
        : data (std::move (x.data)),
          str_cache (std::move (x.str_cache)),
          str_valid (x.str_valid),
          idx (std::move (x.idx))
      {
      }

      line&
      operator = (line&& x) noexcept
      {
        if (this != &x)
        {
          data = std::move (x.data);
          str_cache = std::move (x.str_cache);
          str_valid = x.str_valid;
          idx = std::move (x.idx);
        }
        return *this;
      }

      bool
      operator== (const line& x) const
      {
        // Equality is defined purely by content. If the text is the same,
        // the grapheme boundaries are mathematically guaranteed to be the
        // same.
        //
        return data == x.data;
      }

      std::string_view
      view () const
      {
        if (!str_valid)
        {
          str_cache.clear ();
          str_cache.reserve (data.size ());
          immer::for_each_chunk (data,
                                 [&] (auto first, auto last)
          {
            str_cache.append (first, last);
          });
          str_valid = true;
        }
        return str_cache;
      }

      void
      update_idx ()
      {
        idx.update (view ());
      }

      std::size_t
      count () const
      {
        return idx.size ();
      }
    };

    using lines_type = immer::flex_vector<line>;

    // We must ensure the buffer is never in an invalid state. An "empty" file
    // technically contains one line with zero characters.
    //
    text_buffer ()
      : lines_ ({line {}})
    {
    }

    explicit
    text_buffer (lines_type ls)
        : lines_ (std::move (ls))
    {
      MINE_INVARIANT (!lines_.empty ());
    }

    // Queries
    //

    std::size_t
    line_count () const noexcept
    {
      return lines_.size ();
    }

    // Check bounds. Usually called by assertions, but sometimes useful for
    // cursor validation logic in the UI layer.
    //
    bool
    contains (line_number ln) const noexcept
    {
      return ln.value < lines_.size ();
    }

    const line&
    line_at (line_number ln) const
    {
      MINE_PRECONDITION (contains (ln));
      return lines_[ln.value];
    }

    std::size_t
    line_length (line_number ln) const
    {
      MINE_PRECONDITION (contains (ln));
      return lines_[ln.value].count ();
    }

    // Retrieve the actual UTF-8 bytes for a specific grapheme.
    //
    // This is useful for rendering or for inspecting what character is under
    // the cursor. Note that we return a string_view into the immer storage,
    // which is contiguous for small chunks but we should be careful if we
    // ever switch to a chunked list implementation (though flex_vector handles
    // this gracefully for now).
    //
    std::optional<std::string_view>
    grapheme_at (cursor_position p) const
    {
      if (!contains (p.line))
        return std::nullopt;

      const auto& l (lines_[p.line.value]);
      const auto* c (l.idx.cluster_at_index (p.column.value));

      if (c == nullptr)
        return std::nullopt;

      return c->text (l.view ());
    }

    // Mutations (Pure)
    //
    // In the spirit of functional programming, these functions are pure. They
    // take the current state and a mutation request, and return a brand new
    // `text_buffer` object. The old object remains valid (which is great for
    // recursive operations or background saves).
    //

    // Insert text into the middle of a line.
    //
    // This is a bit tricky because we have to map the high-level concept of
    // "cursor column" (graphemes) down to the low-level reality of "byte
    // offset" in the UTF-8 buffer.
    //
    text_buffer
    insert_graphemes (cursor_position p, std::string_view s) const
    {
      MINE_PRECONDITION (contains (p.line));

      const auto& l (lines_[p.line.value]);
      MINE_PRECONDITION (p.column.value <= l.count ());

      // First, translate the logical grapheme index to a byte offset.
      //
      std::size_t n (l.idx.index_to_byte (p.column.value));

      // Now we perform surgery on the immutable vector. We slice it into
      // prefix (before cursor) and suffix (after cursor).
      //
      auto pre (l.data.take (n));
      auto suf (l.data.drop (n));

      immer::flex_vector<char> v (s.begin (), s.end ());

      // Stitch it back together: prefix + new_text + suffix.
      //
      // The `line` constructor will run the grapheme segmentation algorithm
      // on the result immediately.
      //
      auto t (pre + v + suf);
      line nl (std::move (t));

      // Replace the old line in the line-vector with our new one.
      //
      return text_buffer (lines_.set (p.line.value, std::move (nl)));
    }

    // Handle the Backspace key.
    //
    // This has two distinct behaviors depending on where the cursor is. If
    // we are inside a line, we just shrink that line. If we are at the far
    // left edge (column 0), we have to merge the current line with the
    // previous one.
    //
    text_buffer
    delete_previous_grapheme (cursor_position p) const
    {
      MINE_PRECONDITION (contains (p.line));

      // Case 1: Start of line -> Merge.
      //
      if (p.column.value == 0)
      {
        if (p.line.value == 0)
          return *this; // Top of file, nothing to delete.

        return merge_lines (line_number (p.line.value - 1), p.line);
      }

      // Case 2: Inside line -> Remove grapheme.
      //
      const auto& l (lines_[p.line.value]);

      // We need to find exactly which bytes correspond to the grapheme
      // *before* the cursor.
      //
      const auto* c (l.idx.cluster_at_index (p.column.value - 1));
      MINE_INVARIANT (c != nullptr);

      // Erase that byte range.
      //
      auto t (l.data.erase (c->byte_offset,
                            c->byte_offset + c->byte_length));
      line nl (std::move (t));

      return text_buffer (lines_.set (p.line.value, std::move (nl)));
    }

    // Handle the Delete key.
    //
    // Symmetric to above, but looks forward. If we are at the end of the line,
    // we merge with the next line (pulling it up).
    //
    text_buffer
    delete_next_grapheme (cursor_position p) const
    {
      MINE_PRECONDITION (contains (p.line));

      const auto& l (lines_[p.line.value]);

      // Case 1: End of line -> Merge with next.
      //
      if (p.column.value >= l.count ())
      {
        if (p.line.value + 1 >= lines_.size ())
          return *this; // End of file.

        return merge_lines (p.line, line_number (p.line.value + 1));
      }

      // Case 2: Inside line -> Remove current grapheme.
      //
      const auto* c (l.idx.cluster_at_index (p.column.value));
      MINE_INVARIANT (c != nullptr);

      auto t (l.data.erase (c->byte_offset,
                            c->byte_offset + c->byte_length));
      line nl (std::move (t));

      return text_buffer (lines_.set (p.line.value, std::move (nl)));
    }

    // This is essentially a "split" operation. We take the current line and
    // break it into two separate lines at the cursor position.
    //
    text_buffer
    insert_newline (cursor_position p) const
    {
      MINE_PRECONDITION (contains (p.line));

      const auto& l (lines_[p.line.value]);
      MINE_PRECONDITION (p.column.value <= l.count ());

      std::size_t n (l.idx.index_to_byte (p.column.value));

      // Slice the data.
      //
      auto lhs (l.data.take (n));
      auto rhs (l.data.drop (n));

      line ll (std::move (lhs));
      line rl (std::move (rhs));

      // Update structure:
      //
      // 1. Overwrite the current line with the left-hand side.
      // 2. Insert the right-hand side as a new line immediately following.
      //
      auto r (lines_);
      r = r.set (p.line.value, std::move (ll));
      r = r.insert (p.line.value + 1, std::move (rl));

      return text_buffer (std::move (r));
    }

    // Delete a generic range of text.
    //
    // This is the workhorse for selection deletion. It has to handle the nasty
    // case where the user selects from the middle of line A to the middle of
    // line Z.
    //
    text_buffer
    delete_range (cursor_position b, cursor_position e) const
    {
      MINE_PRECONDITION (b.line.value <= e.line.value);
      MINE_PRECONDITION (contains (b.line));
      MINE_PRECONDITION (contains (e.line));

      // Optimization: If the range is contained entirely within a single line,
      // we don't need to touch the line vector structure, just update one line.
      //
      if (b.line == e.line)
      {
        if (b.column == e.column)
          return *this; // Empty range.

        const auto& l (lines_[b.line.value]);

        std::size_t b_off (l.idx.index_to_byte (b.column.value));
        std::size_t e_off (l.idx.index_to_byte (e.column.value));

        auto t (l.data.erase (b_off, e_off));
        line nl (std::move (t));

        return text_buffer (lines_.set (b.line.value, std::move (nl)));
      }

      // Multi-line case.
      //
      // The strategy is:
      //
      // 1. Take the prefix of the first line (up to start-of-selection).
      // 2. Take the suffix of the last line (from end-of-selection).
      // 3. Glue them together into a single "fused" line.
      // 4. Delete all the lines in between.
      //
      const auto& bl (lines_[b.line.value]);
      const auto& el (lines_[e.line.value]);

      std::size_t b_off (bl.idx.index_to_byte (b.column.value));
      std::size_t e_off (el.idx.index_to_byte (e.column.value));

      auto pre (bl.data.take (b_off));
      auto suf (el.data.drop (e_off));

      auto t (pre + suf);
      line nl (std::move (t));

      // Replace start line with the fused line.
      //
      auto r (lines_.set (b.line.value, std::move (nl)));

      // Qestion: How many lines do we need to kill?
      //
      // Answer: It's the diff between end and start. But also, the end line has
      // its suffix absorbed into the start line.
      //
      auto r_pre (r.take (b.line.value + 1));
      auto r_suf (r.drop (e.line.value + 1));

      return text_buffer (r_pre + r_suf);
    }

    std::string
    get_range (cursor_position b, cursor_position e) const
    {
      MINE_PRECONDITION (b.line.value <= e.line.value);
      MINE_PRECONDITION (contains (b.line));
      MINE_PRECONDITION (contains (e.line));

      if (b.line == e.line)
      {
        // Bail out early if the range is completely empty.
        //
        if (b.column == e.column)
          return "";

        // We are spanning a single line, so just extract the substring between
        // the two byte offsets.
        //
        const auto& l (lines_[b.line.value]);

        std::size_t bo (l.idx.index_to_byte (b.column.value));
        std::size_t eo (l.idx.index_to_byte (e.column.value));

        return std::string (l.view ().substr (bo, eo - bo));
      }

      std::string r;

      // Handle the beginning line. We extract everything from the starting
      // column offset to the end of the line, and tack on the trailing newline.
      //
      const auto& bl (lines_[b.line.value]);
      std::size_t bo (bl.idx.index_to_byte (b.column.value));

      r.append (bl.view ().substr (bo));
      r.push_back ('\n');

      // Append any intermediate lines entirely. Notice we include the newline
      // characters to reconstruct the exact spanned block. We use for_each
      // over a fast structurally shared slice.
      //
      immer::for_each (
        lines_.drop (b.line.value + 1).take (e.line.value - b.line.value - 1),
        [&] (const line& l)
      {
        r.append (l.view ());
        r.push_back ('\n');
      });

      // Finally, process the ending line up to the column offset. We don't
      // append a newline here since the range terminates mid-line.
      //
      const auto& el (lines_[e.line.value]);
      std::size_t eo (el.idx.index_to_byte (e.column.value));

      r.append (el.view ().substr (0, eo));

      return r;
    }

    const lines_type&
    lines () const noexcept
    {
      return lines_;
    }

    bool
    operator== (const text_buffer& x) const noexcept
    {
      return lines_ == x.lines_;
    }

  private:
    lines_type lines_;

    // Helper: Join two consecutive lines.
    //
    // This is used by backspace (at line start) and delete (at line end). It
    // takes line N and N+1 and fuses them into a single line at index N.
    //
    text_buffer
    merge_lines (line_number f, line_number s) const
    {
      MINE_PRECONDITION (f.value + 1 == s.value);
      MINE_PRECONDITION (contains (f));
      MINE_PRECONDITION (contains (s));

      const auto& fl (lines_[f.value]);
      const auto& sl (lines_[s.value]);

      // Simply concatenate the byte vectors.
      //
      auto t (fl.data + sl.data);
      line nl (std::move (t));

      // Set the first line to the merged result, then remove the second line.
      //
      auto r (lines_.set (f.value, std::move (nl)));
      r = r.erase (s.value);

      return text_buffer (std::move (r));
    }
  };

  // Factories
  //

  inline text_buffer
  make_empty_buffer ()
  {
    return text_buffer ();
  }

  // Parse a standard C++ string (or memory mapped file view) into our
  // internal line structure.
  //
  inline text_buffer
  make_buffer_from_string (std::string_view s)
  {
    text_buffer::lines_type ls;

    std::size_t b (0);
    std::size_t e (s.find ('\n'));

    // Scan through the string finding newlines.
    //
    // Note that we don't strictly handle CRLF here, we assume LF. If we wanted
    // full cross-platform support we'd need a smarter split, but for now we
    // assume the input has been normalized or we accept the CR as a character.
    //
    while (e != std::string_view::npos)
    {
      auto sub (s.substr (b, e - b));
      text_buffer::line l (sub);
      ls = ls.push_back (std::move (l));

      b = e + 1;
      e = s.find ('\n', b);
    }

    // Handle the final segment.
    //
    // Even if the string ends with \n, there is technically an empty line
    // after it in many editors' logic, or we just take the last chunk.
    //
    auto sub (s.substr (b));
    text_buffer::line l (sub);
    ls = ls.push_back (std::move (l));

    return text_buffer (std::move (ls));
  }

  // Serialize the buffer back to a single string.
  //
  inline std::string
  buffer_to_string (const text_buffer& b)
  {
    std::string r;
    bool first (true);

    // Iterate over all lines and append them to the result, injecting the
    // newlines back in that we stripped during parsing.
    //
    immer::for_each (b.lines (),
                     [&] (const text_buffer::line& l)
    {
      if (!first)
        r.push_back ('\n');

      first = false;
      auto v = l.view ();

      r.append (v.begin (), v.end ());
    });

    return r;
  }
}
