#pragma once

#include <cstddef>
#include <string_view>

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

  class text_buffer;
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
  assert_cursor_aligned (const cursor& c, const text_buffer& b);

  // Weaker check: just validate coordinates are within array bounds.
  //
  void
  assert_cursor_valid (const cursor_position& p, const text_buffer& b);

  // Verify the integrity of the line cache.
  //
  // This re-runs the segmentation algorithm on the raw text and compares it
  // against the cached `grapheme_index`. If they differ, our incremental
  // update logic is buggy.
  //
  void
  assert_grapheme_index_consistent (std::string_view s, std::size_t count);
}
