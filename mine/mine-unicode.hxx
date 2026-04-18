#pragma once

#include <compare>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <immer/vector.hpp>

#include <unicode/brkiter.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

namespace mine
{
  // Unicode Correctness Assertions
  //
  // Text processing is fragile. A single invalid byte or a misplaced index
  // can cascade into memory corruption or infinite loops in the rendering
  // layer.
  //
  // These assertions are the "immune system" of the editor. They are heavy,
  // usually involving linear scans or ICU calls, so they are strictly compiled
  // out in release builds (`-DNDEBUG`). In debug builds, however, they enforce
  // that every pointer and offset is semantically valid.
  //

  // Verify that a string is well-formed UTF-8.
  //
  // We generally trust inputs from `mine-io` and `mine-terminal-input`, but
  // when slicing strings for inserts/deletes, it's easy to accidentally
  // create invalid sequences.
  //
  void
  assert_valid_utf8 (std::string_view s);

  // Verify that a byte offset falls on a Grapheme Cluster boundary.
  //
  // The cursor must never, ever point to the middle of a grapheme (e.g.,
  // between 'e' and its acute accent). If it does, a backspace operation would
  // delete half the character, leaving the buffer in a corrupted state.
  //
  void
  assert_grapheme_boundary (std::string_view s, std::size_t off);

  // Verify that a byte offset falls on a Code Point boundary.
  //
  // This is a weaker check than grapheme alignment. It ensures we aren't
  // pointing at a UTF-8 continuation byte (10xxxxxx).
  //
  void
  assert_codepoint_boundary (std::string_view s, std::size_t off);

  // Verify ICU calls.
  //
  // ICU uses "out parameters" for error codes. We wrap this to crash hard
  // if the internal engine fails, as it usually implies a configuration error
  // (missing data file) or memory exhaustion.
  //
  void
  assert_icu_success (int code, const char* op);

  // Buffer & Cursor Consistency
  //

  class content;
  class cursor;
  struct cursor_position;

  // Verify that the cursor points to a valid semantic location.
  //
  // This checks two things:
  // 1. The cursor is within the physical bounds of the buffer (lines/cols).
  // 2. The cursor column aligns with a cached grapheme boundary in the
  //    line's index.
  //
  void
  assert_cursor_aligned (const cursor&, const content&);

  // Weaker check: just validate coordinates are within array bounds.
  //
  void
  assert_cursor_valid (const cursor_position&, const content&);

  // Verify the integrity of the line cache.
  //
  // This re-runs the segmentation algorithm on the raw text and compares it
  // against the cached `grapheme_index`. If they differ, our incremental
  // update logic is buggy.
  //
  void
  assert_grapheme_index_consistent (std::string_view s, std::size_t count);

  // Sanitary wrapper around ICU's BreakIterator.
  //
  class icu_break_iterator
  {
  public:
    enum class type
    {
      character,  // Grapheme clusters (user-perceived characters).
      word,       // Word boundaries (for Ctrl+Left/Right).
      line,       // Soft wrap points.
      sentence    // Sentence navigation.
    };

    // Construct the iterator.
    //
    // This will throw or assert if ICU fails to initialize (e.g., missing
    // data files). We default to the system locale because text editing
    // rules are often locale-dependent (e.g., word breaking in Thai vs
    // English).
    //
    explicit
    icu_break_iterator (type,
                        const icu::Locale& l = icu::Locale::getDefault ());

    // Move-only.
    //
    // ICU iterators are heavy objects with internal state caches. Copying them
    // is expensive and rarely what you want.
    //
    icu_break_iterator (const icu_break_iterator&) = delete;
    icu_break_iterator& operator= (const icu_break_iterator&)= delete;

    icu_break_iterator (icu_break_iterator&&) noexcept = default;
    icu_break_iterator& operator= (icu_break_iterator&&) noexcept = default;

    ~icu_break_iterator () = default;

    // Reset the analysis target.
    //
    // Note that this does not copy the string if you pass a persistent
    // `UnicodeString`, but usually we are bridging from `std::string`, so a
    // copy happens before this call.
    //
    void
    set_text (const icu::UnicodeString&);

    // Navigation
    //
    // These wrap the raw ICU calls, returning `BreakIterator::DONE` (-1) when
    // boundaries are exhausted.
    //

    int32_t
    first ();

    int32_t
    last ();

    int32_t
    next ();

    int32_t
    previous ();

    int32_t
    current () const;

    // Smart navigation.
    //

    // finds the first boundary strictly after `n`.
    //
    int32_t
    following (int32_t off);

    // finds the first boundary strictly before `n`.
    //
    int32_t
    preceding (int32_t off);

    // Is the given offset a valid boundary?
    //
    bool
    is_boundary (int32_t off);

    // Escape hatch for raw API access.
    //
    template <typename Self>
    auto&&
    get (this Self&& self)
    {
      return std::forward_like<Self> (*self.iter_);
    }

    bool
    valid () const;

  private:
    std::unique_ptr<icu::BreakIterator> iter_;
  };

  // The Bridge Tax (UTF-8 <-> UTF-16)
  //
  // Our editor engine is pure UTF-8 (`std::string`), but ICU is native to
  // UTF-16 (`icu::UnicodeString`). Every time we want to ask ICU a question, we
  // have to pay the cost of conversion.
  //
  // These utilities handle the impedance mismatch. They are not free.
  //

  // Inflate UTF-8 to UTF-16.
  //
  // Performs a heap allocation. Avoid calling this in tight loops; prefer
  // converting once and reusing the `UnicodeString` if possible.
  //
  icu::UnicodeString
  utf8_to_ustring (std::string_view s);

  // Map a UTF-16 offset back to a UTF-8 byte offset.
  //
  // Since variable-length encodings don't map linearly, this requires walking
  // the string from the beginning (or a known sync point), counting code
  // points.
  //
  // Complexity: O(N) relative to the offset.
  //
  std::size_t
  utf16_offset_to_utf8 (const icu::UnicodeString&,
                        std::string_view s,
                        int32_t u_off);

  // Map a UTF-8 byte offset to a UTF-16 offset.
  //
  // Complexity: O(N) relative to the offset.
  //
  int32_t
  utf8_offset_to_utf16 (const icu::UnicodeString&,
                        std::string_view s,
                        std::size_t b_off);

  // A single "User Perceived Character".
  //
  // In the Unicode world, what a user deletes with one backspace press might
  // be a single byte (ASCII 'a'), two bytes (Latin-1 'é'), three bytes
  // (Kanji), or even a sequence of multiple code points (Flag Emoji or
  // Family Emoji).
  //
  // This structure captures the physical byte range of such a unit within a
  // larger buffer.
  //
  struct grapheme_cluster
  {
    std::size_t byte_offset = 0;
    std::size_t byte_length = 0;
    std::size_t index       = 0; // The logical column number (0, 1, 2...).

    // Convenience helper to extract the actual bytes.
    //
    std::string_view
    text (const std::string_view full_text) const;

    bool operator== (const grapheme_cluster&) const = default;
  };

  // The topology of a UTF-8 string.
  //
  // We compute this once and store it. This allows us to map between "Screen
  // Column 5" and "Byte Offset 12" without re-running the heavy ICU state
  // machine for every lookup.
  //
  struct grapheme_segmentation
  {
    immer::vector<grapheme_cluster> clusters;
    std::size_t                     total_bytes = 0;

    // Find the cluster that physically contains the given byte offset.
    //
    // Returns nullptr if the offset is out of bounds. This uses a binary
    // search, so it's O(log N).
    //
    const grapheme_cluster*
    find_at_byte (std::size_t off) const;

    // Find the cluster at the logical index `i`.
    //
    // Returns nullptr if `i` is out of bounds. This is a direct vector lookup,
    // O(1).
    //
    const grapheme_cluster*
    at_index (std::size_t i) const;

    // Map physical byte offset -> logical grapheme index.
    //
    std::size_t
    byte_to_index (std::size_t off) const;

    // Map logical grapheme index -> physical byte offset.
    //
    std::size_t
    index_to_byte (std::size_t i) const;

    std::size_t
    size () const;

    bool
    empty () const;
  };

  // Algorithms
  //

  // The Heavy Lifter.
  //
  // Runs the ICU BreakIterator over the entire string and produces a linear map
  // of all grapheme boundaries. This involves memory allocation and string
  // conversion (UTF-8 to UTF-16), so use it sparingly (e.g., after the user
  // presses Enter or finishes a paste).
  //
  grapheme_segmentation
  segment_graphemes (std::string_view s);

  // The Lightweight Counter.
  //
  // If you just need to know "how wide is this string?" without storing the
  // topology, use this. It iterates through the string without allocating a
  // result vector.
  //
  std::size_t
  count_graphemes (std::string_view s);

  // Boundary Navigation (Stateless)
  //
  // These functions spin up a temporary ICU iterator to answer a single
  // question. Useful for small queries or one-off checks, but if you are
  // processing a whole string, use `segment_graphemes` instead.
  //

  // Find the start of the next grapheme cluster strictly after `off`.
  //
  std::size_t
  next_grapheme_boundary (std::string_view s, std::size_t off);

  // Find the start of the grapheme cluster strictly before `off`.
  //
  std::size_t
  prev_grapheme_boundary (std::string_view s, std::size_t off);

  // Check if `off` aligns exactly with the start of a grapheme.
  //
  bool
  is_grapheme_boundary (std::string_view s, std::size_t off);

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
    valid () const;

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
    apply (grapheme_index&, std::string_view s);
  };

  // A lightweight cursor over pre-computed grapheme clusters.
  //
  // Architectural Note:
  //
  // We decoupled the "discovery" of graphemes (ICU BreakIterator, which is
  // heavy and stateful) from the "traversal" (this iterator). We now rely
  // entirely on the high-performance iterators provided by immer.
  //
  using grapheme_iterator = immer::vector<grapheme_cluster>::const_iterator;

  // Syntactic sugar for range-based for loops.
  //
  class grapheme_range
  {
  public:
    explicit
    grapheme_range (const grapheme_segmentation& s) noexcept;

    [[nodiscard]] grapheme_iterator
    begin () const noexcept;

    [[nodiscard]] grapheme_iterator
    end () const noexcept;

    [[nodiscard]] std::size_t
    size () const noexcept;

    [[nodiscard]] bool
    empty () const noexcept;

  private:
    const grapheme_segmentation* s_;
  };

  // Factory.
  //
  [[nodiscard]] inline grapheme_range
  make_grapheme_range (const grapheme_segmentation& s) noexcept
  {
    return grapheme_range (s);
  }
}
