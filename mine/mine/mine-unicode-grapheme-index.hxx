#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <optional>

#include <mine/mine-unicode-grapheme.hxx>

namespace mine
{
  // A cached mapping between physical bytes and logical graphemes.
  //
  // Why do we need this?
  //
  // ICU's `BreakIterator` is correct but heavy. We cannot afford to spin up a
  // new iterator and walk the string every time the cursor moves or a character
  // is rendered.
  //
  // This class acts as a cache. It takes the raw text, computes the
  // segmentation once (via `update()`), and then provides lookups for things
  // like "what is the 5th character?" or "where does the character at byte 10
  // end?".
  //
  // The trade-off is memory. We are essentially storing a metadata vector
  // parallel to the text.
  //
  class grapheme_index
  {
  public:
    grapheme_index () = default;

    // Movable but not copyable.
    //
    // Copying a segmentation cache is usually a mistake (if you have the string
    // elsewhere, just recompute it; if you are copying the string, you likely
    // need a new cache anyway).
    //
    grapheme_index (const grapheme_index&) = delete;
    grapheme_index& operator= (const grapheme_index&) = delete;

    grapheme_index (grapheme_index&&) noexcept = default;
    grapheme_index& operator= (grapheme_index&&) noexcept = default;

    ~grapheme_index () = default;

    // Rebuild the index.
    //
    // This triggers the heavy ICU machinery. You should call this immediately
    // after modifying the underlying text buffer.
    //
    void
    update (std::string_view s);

    // Mark the cache as dirty.
    //
    // Useful if we want to defer computation until the next read access,
    // though generally we prefer eager updates to keep the UI snappy.
    //
    void
    invalidate ();

    // Is the cache warm?
    //
    bool
    valid () const
    {
      return segmentation_.has_value ();
    }

    // Access the raw data.
    //
    const grapheme_segmentation&
    get_segmentation () const;

    // Conversions
    //

    // Map a byte offset to a logical grapheme index (0-based column).
    //
    std::size_t
    byte_to_index (std::size_t off) const;

    // Map a logical grapheme index to the byte offset where it starts.
    //
    std::size_t
    index_to_byte (std::size_t i) const;

    // Lookup
    //

    // Find the cluster containing the byte at `off`.
    //
    const grapheme_cluster*
    cluster_at_byte (std::size_t off) const;

    // Find the cluster at logical position `i`.
    //
    const grapheme_cluster*
    cluster_at_index (std::size_t i) const;

    // Metrics
    //

    // How many graphemes are in the string?
    //
    std::size_t
    size () const;

    // How many bytes does the string occupy?
    //
    std::size_t
    byte_length () const;

    // Boundary Navigation
    //

    // Find the nearest grapheme boundary strictly after `off`.
    //
    std::size_t
    next_boundary (std::size_t off) const;

    // Find the nearest grapheme boundary strictly before `off`.
    //
    std::size_t
    prev_boundary (std::size_t off) const;

    // Is `off` exactly at the start of a grapheme cluster?
    //
    bool
    is_boundary (std::size_t off) const;

  private:
    // We use std::optional to represent the "invalid/empty" state cheaply.
    //
    mutable std::optional<grapheme_segmentation> segmentation_;
  };

  // Context for incremental updates.
  //
  // Currently, `grapheme_index::update` is a "nuclear option", it rebuilds
  // everything. For very large lines (10k+ chars), this might lag.
  //
  // This struct is a placeholder for a smarter algorithm that would:
  //
  // 1. Identify the range of bytes changed.
  // 2. Find safe cut points (grapheme boundaries) before and after the edit.
  // 3. Re-segment only the dirty middle part.
  // 4. Shift the indices of the tail part.
  //
  // (Not yet implemented, but the API hook is here).
  //
  struct grapheme_edit_context
  {
    std::size_t start; // Byte offset of edit start.
    std::size_t end;   // Byte offset of edit end (old text).
    std::size_t len;   // Byte length of new text.

    bool
    apply (grapheme_index& idx, std::string_view s);
  };
}
