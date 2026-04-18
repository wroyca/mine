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
#include <mine/mine-contract.hxx>
#include <mine/mine-unicode.hxx>

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
      line (immer::flex_vector<char> d);

      explicit
      line (std::string_view s);

      line (const line& x);
      line& operator= (const line& x);
      line (line&& x) noexcept;
      line& operator= (line&& x) noexcept;

      bool operator== (const line& x) const;

      std::string_view
      view () const;

      void
      update_idx ();

      const grapheme_index&
      get_index () const;

      std::size_t
      count () const;
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

    std::optional<std::string_view>
    grapheme_at (cursor_position p) const;

    // Mutations (Pure)
    //

    text_buffer
    insert_graphemes (cursor_position p, std::string_view s) const;

    text_buffer
    delete_previous_grapheme (cursor_position p) const;

    text_buffer
    delete_next_grapheme (cursor_position p) const;

    text_buffer
    insert_newline (cursor_position p) const;

    text_buffer
    delete_range (cursor_position b, cursor_position e) const;

    std::string
    get_range (cursor_position b, cursor_position e) const;

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

    text_buffer
    merge_lines (line_number f, line_number s) const;
  };

  // Factories
  //

  text_buffer
  make_empty_buffer ();

  text_buffer
  make_buffer_from_string (std::string_view s);

  std::string
  buffer_to_string (const text_buffer& b);
}
