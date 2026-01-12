#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <string_view>

namespace mine
{
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
    text (std::string_view full_text) const
    {
      return full_text.substr (byte_offset, byte_length);
    }

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
    std::vector<grapheme_cluster> clusters;
    std::size_t                   total_bytes = 0;

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
    size () const
    {
      return clusters.size ();
    }

    bool
    empty () const
    {
      return clusters.empty ();
    }
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
}
