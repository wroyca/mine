#include <mine/mine-unicode-grapheme.hxx>
#include <mine/mine-unicode-icu.hxx>
#include <mine/mine-unicode-assert.hxx>

#include <unicode/brkiter.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <vector>

using namespace std;

namespace mine
{
  // Segmentation Logic
  //

  const grapheme_cluster* grapheme_segmentation::
  find_at_byte (size_t off) const
  {
    if (off > total_bytes)
      return nullptr;

    // We store the clusters in a contiguous vector, strictly sorted by their
    // byte_offset. This allows us to perform a binary search to locate the
    // cluster containing any arbitrary byte.
    //
    // We use `upper_bound` to find the first cluster that starts strictly
    // *after* our target offset. The cluster immediately preceding that one
    // must be the one that contains our offset (or it's a hole).
    //
    auto it (upper_bound (
      clusters.begin (),
      clusters.end (),
      off,
      [] (size_t val, const grapheme_cluster& c)
      {
        return val < c.byte_offset;
      }));

    // If the iterator is at the beginning, it means every single cluster
    // starts after our offset. This implies `off` is before the first
    // cluster (which shouldn't happen if `off >= 0` and the list is valid).
    //
    if (it != clusters.begin ())
      --it;

    // Now we have a candidate cluster. We must verify that `off` actually
    // falls within its range [start, start + length).
    //
    if (it != clusters.end () &&
        off >= it->byte_offset &&
        off < it->byte_offset + it->byte_length)
      return &(*it);

    return nullptr;
  }

  const grapheme_cluster* grapheme_segmentation::
  at_index (size_t i) const
  {
    return i < clusters.size ()
      ? &clusters[i]
      : nullptr;
  }

  size_t grapheme_segmentation::
  byte_to_index (size_t off) const
  {
    const auto* c (find_at_byte (off));
    return c ? c->index : clusters.size ();
  }

  size_t grapheme_segmentation::
  index_to_byte (size_t i) const
  {
    const auto* c (at_index (i));
    return c ? c->byte_offset : total_bytes;
  }

  // Algorithm Implementation
  //

  grapheme_segmentation
  segment_graphemes (string_view s)
  {
    assert_valid_utf8 (s);

    grapheme_segmentation res;
    res.total_bytes = s.size ();

    if (s.empty ())
      return res;

    // The ICU Dance.
    //
    // This is where the impedance mismatch bites us. Our editor is pure UTF-8
    // (`char`), but ICU's algorithms are native to UTF-16 (`UChar`).
    //
    // To find boundaries, we are forced to:
    // 1. Allocate a temporary buffer and convert the UTF-8 string to UTF-16.
    // 2. Feed that UTF-16 string into the ICU BreakIterator.
    // 3. Receive boundary results as UTF-16 offsets.
    // 4. Map those UTF-16 offsets back to the original UTF-8 byte offsets.
    //
    // This round-trip is expensive, which is why we cache the result in a
    // `grapheme_index` rather than re-running this on every frame.
    //
    auto ustr (utf8_to_ustring (s));

    icu_break_iterator iter (icu_break_iterator::type::character);
    iter.set_text (ustr);

    int32_t start (iter.first ());
    int32_t end (iter.next ());

    size_t idx (0);

    // Iterate through the boundaries found by ICU.
    //
    while (end != icu::BreakIterator::DONE)
    {
      // Convert the UTF-16 boundary offsets back to physical UTF-8 byte offsets
      // so we can store them in our cluster struct.
      //
      // Note: `utf16_offset_to_utf8` currently does a scan from the beginning
      // or a heuristic guess. Since we are moving monotonically forward, we
      // could optimize this to be incremental, but for now correctness is the
      // priority.
      //
      size_t b_start (utf16_offset_to_utf8 (ustr, s, start));
      size_t b_end   (utf16_offset_to_utf8 (ustr, s, end));

      grapheme_cluster c;
      c.byte_offset = b_start;
      c.byte_length = b_end - b_start;
      c.index       = idx++;

      res.clusters.push_back (c);

      start = end;
      end = iter.next ();
    }

    return res;
  }

  size_t
  count_graphemes (string_view s)
  {
    // If we only need the count, we can skip allocating the cluster vector
    // and just pump the iterator.
    //
    assert_valid_utf8 (s);

    if (s.empty ())
      return 0;

    auto ustr (utf8_to_ustring (s));

    icu_break_iterator iter (icu_break_iterator::type::character);
    iter.set_text (ustr);

    size_t n (0);
    iter.first ();

    while (iter.next () != icu::BreakIterator::DONE)
      ++n;

    return n;
  }

  size_t
  next_grapheme_boundary (string_view s, size_t off)
  {
    assert_valid_utf8 (s);
    assert (off <= s.size ());

    // Boundary check: if we are already at the end, we can't go further.
    //
    if (off >= s.size ())
      return s.size ();

    auto ustr (utf8_to_ustring (s));

    // Map our input byte-offset to a UChar-offset so ICU understands it.
    //
    int32_t u_off (utf8_offset_to_utf16 (ustr, s, off));

    // Spin up a fresh iterator.
    //
    // Warning: This instantiation is not free. Do not use this function
    // inside a tight loop (like rendering every character on a line). Use
    // the segmented index for that. This function is meant for one-off
    // operations like "delete forward".
    //
    icu_break_iterator iter (icu_break_iterator::type::character);
    iter.set_text (ustr);

    // Ask ICU for the boundary strictly following our current position.
    //
    int32_t next (iter.following (u_off));

    if (next == icu::BreakIterator::DONE)
      return s.size ();

    // Map the result back to bytes.
    //
    return utf16_offset_to_utf8 (ustr, s, next);
  }

  size_t
  prev_grapheme_boundary (string_view s, size_t off)
  {
    assert_valid_utf8 (s);
    assert (off <= s.size ());

    if (off == 0)
      return 0;

    auto ustr (utf8_to_ustring (s));
    int32_t u_off (utf8_offset_to_utf16 (ustr, s, off));

    icu_break_iterator iter (icu_break_iterator::type::character);
    iter.set_text (ustr);

    int32_t prev (iter.preceding (u_off));

    if (prev == icu::BreakIterator::DONE)
      return 0;

    return utf16_offset_to_utf8 (ustr, s, prev);
  }

  bool
  is_grapheme_boundary (string_view s, size_t off)
  {
    assert_valid_utf8 (s);

    if (off == 0 || off == s.size ())
      return true;

    if (off > s.size ())
      return false;

    auto ustr (utf8_to_ustring (s));
    int32_t u_off (utf8_offset_to_utf16 (ustr, s, off));

    icu_break_iterator iter (icu_break_iterator::type::character);
    iter.set_text (ustr);

    return iter.is_boundary (u_off);
  }
}
