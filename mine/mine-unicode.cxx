#include <mine/mine-unicode.hxx>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string_view>

#include <immer/vector_transient.hpp>

#include <unicode/stringpiece.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>

#include <mine/mine-core-buffer.hxx>
#include <mine/mine-core-cursor.hxx>

using namespace std;

namespace mine
{
  void
  assert_valid_utf8 (string_view s)
  {
#ifndef NDEBUG
    if (s.empty ())
      return;

    // We abuse the "pre-flighting" feature of ICU here.
    //
    // u_strFromUTF8 usually converts text. By passing NULL as the destination
    // and 0 as capacity, we ask it "how much buffer do I need?". As a side
    // effect, it validates the input sequence.
    //
    UErrorCode e (U_ZERO_ERROR);
    int32_t n (0);

    u_strFromUTF8 (nullptr,                       // dest
                   0,                             // destCapacity
                   &n,                            // pDestLength
                   s.data (),                     // src
                   static_cast<int32_t> (s.size ()),
                   &e);

    // U_BUFFER_OVERFLOW_ERROR is actually success here (it means the input
    // was valid but didn't fit in our 0-size buffer). Anything else is a
    // data corruption problem.
    //
    assert ((e == U_ZERO_ERROR || e == U_BUFFER_OVERFLOW_ERROR) &&
            "Invalid UTF-8 sequence detected");
#else
    (void)s;
#endif
  }

  void
  assert_grapheme_boundary (string_view s, size_t off)
  {
#ifndef NDEBUG
    // Boundaries are always valid.
    //
    if (off == 0 || off == s.size ())
      return;

    assert (off <= s.size () && "Byte offset out of range");

    // First, a cheap sanity check: Are we pointing into the middle of a
    // UTF-8 byte sequence? If so, we can fail fast without bothering ICU.
    //
    assert_codepoint_boundary (s, off);

    UErrorCode e (U_ZERO_ERROR);

    // The naive approach would be to convert the string to UTF-16 to use
    // the standard BreakIterator. That involves a massive allocation and copy.
    //
    // UText allows us to wrap the existing UTF-8 buffer in a view that the
    // BreakIterator understands. This implies that 'off' functions correctly
    // as a native byte index, which is exactly what we need.
    //
    UText* ut (utext_openUTF8 (nullptr,
                               s.data (),
                               static_cast<int64_t> (s.size ()),
                               &e));

    assert (U_SUCCESS (e) && "Failed to open UText for UTF-8");

    // Note: utext_close returns UText*, not void.
    //
    auto ut_guard (unique_ptr<UText, UText* (*) (UText*)> (ut, utext_close));

    auto bi (ubrk_open (UBRK_CHARACTER,
                        nullptr, // default locale
                        nullptr, // text (set later)
                        0,       // text length
                        &e));

    assert (U_SUCCESS (e) && "Failed to create ICU break iterator");

    auto bi_guard (
      unique_ptr<UBreakIterator, void (*) (UBreakIterator*)> (bi, ubrk_close));

    // Bind the text view to the iterator.
    //
    ubrk_setUText (bi, ut, &e);
    assert (U_SUCCESS (e));

    // Finally, check the grapheme break logic.
    //
    bool is_bound (ubrk_isBoundary (bi, static_cast<int32_t> (off)));
    assert (is_bound && "Byte offset splits a grapheme cluster");

#else
    (void)s;
    (void)off;
#endif
  }

  void
  assert_codepoint_boundary (string_view s, size_t off)
  {
#ifndef NDEBUG
    if (off == 0 || off >= s.size ())
      return;

    // Bitwise magic.
    //
    // In UTF-8, continuation bytes always start with bits `10xxxxxx` (0x80).
    // Start bytes are either `0xxxxxxx` (ASCII) or `11xxxxxx` (Multi-byte
    // start).
    //
    // We just verify we aren't pointing at a continuation byte.
    //
    unsigned char b (static_cast<unsigned char> (s[off]));
    assert (((b & 0xC0) != 0x80) &&
            "Byte offset points to middle of UTF-8 code point");
#else
    (void)s;
    (void)off;
#endif
  }

  void
  assert_icu_success (int code, const char* op)
  {
#ifndef NDEBUG
    UErrorCode e (static_cast<UErrorCode> (code));

    if (U_FAILURE (e))
    {
      // We purposefully crash here. If ICU is failing basic operations, our
      // assumptions about the text processing environment are void.
      //
      assert (false && "ICU operation failed");
      (void)op;
    }
#else
    (void)code;
    (void)op;
#endif
  }

  // Editor State Validations
  //

  void
  assert_cursor_aligned (const cursor& c, const text_buffer& b)
  {
#ifndef NDEBUG
    // Check line existence first.
    //
    if (!b.contains (c.line ()))
      return;

    const auto& l (b.line_at (c.line ()));
    size_t n (l.count ());

    // The cursor is allowed to be *at* the end (appending), but not past it.
    //
    assert (c.column ().value <= n &&
            "Cursor beyond end of line");

    // If we are inside the text, we must verify that the column index maps
    // to a real grapheme cluster in our cache.
    //
    if (c.column ().value < n)
    {
      const auto* cl (l.idx.cluster_at_index (c.column ().value));
      assert (cl != nullptr && "Cursor column detached from grapheme index");
    }
#else
    (void)c;
    (void)b;
#endif
  }

  void
  assert_cursor_valid (const cursor_position& p, const text_buffer& b)
  {
#ifndef NDEBUG
    // Basic bounds checking.
    //
    assert (p.line.value < b.line_count () &&
            "Cursor line OOB");

    const auto& l (b.line_at (p.line));
    assert (p.column.value <= l.count () &&
            "Cursor column OOB");
#else
    (void)p;
    (void)b;
#endif
  }

  void
  assert_grapheme_index_consistent (string_view s, size_t count)
  {
#ifndef NDEBUG
    // Trust, but verify.
    //
    // The `grapheme_index` is a cached view of the string. Here we re-run the
    // segmentation algorithm from scratch to verify the cache hasn't drifted
    // from the raw text (which can happen if we mess up the offset math during
    // complex insertions).
    //
    grapheme_segmentation seg (segment_graphemes (s));

    assert (seg.size () == count &&
            "Cached grapheme count disagrees with actual content");

    for (const auto& c : seg.clusters)
    {
      assert (c.byte_offset + c.byte_length <= s.size ());
      assert_grapheme_boundary (s, c.byte_offset);
    }
#else
    (void)s;
    (void)count;
#endif
  }

  // ICU BreakIterator Wrapper
  //

  icu_break_iterator::
  icu_break_iterator (type t, const icu::Locale& l)
  {
    // ICU uses "out parameters" for error handling. We wrap this 90s-style
    // API into something RAII-compliant that asserts on failure.
    //
    UErrorCode e (U_ZERO_ERROR);

    switch (t)
    {
    case type::character:
      iter_.reset (icu::BreakIterator::createCharacterInstance (l, e));
      break;
    case type::word:
      iter_.reset (icu::BreakIterator::createWordInstance (l, e));
      break;
    case type::line:
      iter_.reset (icu::BreakIterator::createLineInstance (l, e));
      break;
    case type::sentence:
      iter_.reset (icu::BreakIterator::createSentenceInstance (l, e));
      break;
    }

    assert_icu_success (e, "BreakIterator creation");
    assert (iter_ != nullptr && "BreakIterator factory returned null");
  }

  void icu_break_iterator::
  set_text (const icu::UnicodeString& s)
  {
    assert (iter_ != nullptr);
    iter_->setText (s);
  }

  int32_t icu_break_iterator::
  first ()
  {
    assert (iter_ != nullptr);
    return iter_->first ();
  }

  int32_t icu_break_iterator::
  last ()
  {
    assert (iter_ != nullptr);
    return iter_->last ();
  }

  int32_t icu_break_iterator::
  next ()
  {
    assert (iter_ != nullptr);
    return iter_->next ();
  }

  int32_t icu_break_iterator::
  previous ()
  {
    assert (iter_ != nullptr);
    return iter_->previous ();
  }

  int32_t icu_break_iterator::
  current () const
  {
    assert (iter_ != nullptr);
    return iter_->current ();
  }

  int32_t icu_break_iterator::
  following (int32_t off)
  {
    assert (iter_ != nullptr);
    return iter_->following (off);
  }

  int32_t icu_break_iterator::
  preceding (int32_t off)
  {
    assert (iter_ != nullptr);
    return iter_->preceding (off);
  }

  bool icu_break_iterator::
  is_boundary (int32_t off)
  {
    assert (iter_ != nullptr);
    return iter_->isBoundary (off);
  }

  bool icu_break_iterator::
  valid () const
  {
    return iter_ != nullptr;
  }

  // Offset Mapping utilities
  //

  icu::UnicodeString
  utf8_to_ustring (string_view s)
  {
    assert_valid_utf8 (s);

    // Zero-copy attempt using StringPiece, though fromUTF8 usually performs
    // a copy into the internal UChar buffer anyway.
    //
    return icu::UnicodeString::fromUTF8 (
      icu::StringPiece (s.data (), static_cast<int32_t> (s.size ())));
  }

  size_t
  utf16_offset_to_utf8 (const icu::UnicodeString& u,
                        string_view s,
                        int32_t off)
  {
    assert (off >= 0 && off <= u.length ());
    assert_valid_utf8 (s);

    if (off == 0)
      return 0;

    if (off == u.length ())
      return s.size ();

    // The Walk.
    //
    // Since UTF-8 and UTF-16 are both variable-length encodings, there is no
    // math formula to jump from one to the other. We must walk the string
    // codepoint by codepoint, advancing two counters.
    //
    // This makes this function O(N).
    //
    size_t byte_off (0);
    int32_t u16_pos (0);

    while (u16_pos < off && u16_pos < u.length ())
    {
      UChar32 cp;
      U16_NEXT (u.getBuffer (), u16_pos, u.length (), cp);

      // U16_NEXT advances u16_pos. Now we manually advance the byte offset
      // based on how many bytes that codepoint takes in UTF-8.
      //
      byte_off += U8_LENGTH (cp);
    }

    assert (byte_off <= s.size ());
    return byte_off;
  }

  int32_t
  utf8_offset_to_utf16 (const icu::UnicodeString& u,
                        string_view s,
                        size_t off)
  {
    assert (off <= s.size ());
    assert_valid_utf8 (s);

    if (off == 0)
      return 0;

    if (off == s.size ())
      return u.length ();

    // The Inverse Walk.
    //
    // Same O(N) penalty as above. This is why `grapheme_index` exists:
    // to cache these calculations so we aren't doing this loop on every
    // keystroke or render cycle.
    //
    size_t byte_pos (0);
    int32_t u16_off (0);

    while (byte_pos < off && u16_off < u.length ())
    {
      UChar32 cp;
      U16_NEXT (u.getBuffer (), u16_off, u.length (), cp);

      byte_pos += U8_LENGTH (cp);
    }

    assert (u16_off <= u.length ());
    return u16_off;
  }

  string_view grapheme_cluster::
  text (const string_view s) const
  {
    return s.substr (byte_offset, byte_length);
  }

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

  size_t grapheme_segmentation::
  size () const
  {
    return clusters.size ();
  }

  bool grapheme_segmentation::
  empty () const
  {
    return clusters.empty ();
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

    auto clusters_t = immer::vector_transient<grapheme_cluster>();

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

      clusters_t.push_back (c);

      start = end;
      end = iter.next ();
    }

    res.clusters = clusters_t.persistent();

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

  void grapheme_index::
  update (string_view s)
  {
    // Trust but verify.
    //
    assert_valid_utf8 (s);

    // The Nuclear Option.
    //
    // Currently, we rebuild the entire index from scratch on every update.
    //
    // Is this slow? Yes, it's O(N).
    //
    // Is it avoidable? Mostly. Ideally, we would detect the range of changes,
    // find a "safe point" before and after the edit (where grapheme state
    // resets), and only re-segment the dirty window.
    //
    // However, text editing is tricky. Inserting a single character can
    // theoretically merge two existing clusters (e.g., adding a combining
    // mark). For now, correctness wins over optimization.
    //
    segmentation_ = segment_graphemes (s);
  }

  void grapheme_index::
  invalidate ()
  {
    segmentation_.reset ();
  }

  bool grapheme_index::
  valid () const
  {
    return segmentation_.has_value ();
  }

  const grapheme_segmentation& grapheme_index::
  get_segmentation () const
  {
    // If we are hitting this assert, we forgot to call update() after
    // constructing the line or modifying the text. The index is not
    // self-updating; it must be explicitly told when the world changes.
    //
    assert (segmentation_.has_value () &&
            "accessing uninitialized grapheme index");

    return *segmentation_;
  }

  size_t grapheme_index::
  byte_to_index (size_t off) const
  {
    return segmentation_
      ? segmentation_->byte_to_index (off)
      : 0;
  }

  size_t grapheme_index::
  index_to_byte (size_t i) const
  {
    return segmentation_
      ? segmentation_->index_to_byte (i)
      : 0;
  }

  const grapheme_cluster* grapheme_index::
  cluster_at_byte (size_t off) const
  {
    return segmentation_
      ? segmentation_->find_at_byte (off)
      : nullptr;
  }

  const grapheme_cluster* grapheme_index::
  cluster_at_index (size_t i) const
  {
    return segmentation_
      ? segmentation_->at_index (i)
      : nullptr;
  }

  size_t grapheme_index::
  size () const
  {
    return segmentation_ ? segmentation_->size () : 0;
  }

  size_t grapheme_index::
  byte_length () const
  {
    return segmentation_ ? segmentation_->total_bytes : 0;
  }

  size_t grapheme_index::
  next_boundary (size_t off) const
  {
    if (!segmentation_)
      return off;

    const auto& seg (*segmentation_);

    // Clamp to EOF.
    //
    if (off >= seg.total_bytes)
      return seg.total_bytes;

    const auto* c (seg.find_at_byte (off));

    if (c == nullptr)
      return seg.total_bytes;

    // The next boundary is *always* the end of the current cluster.
    //
    // If we are at the start of a cluster (off == c->byte_offset), the next
    // logical position is the start of the next cluster.
    //
    // If we are inside a cluster (off > c->byte_offset), the next valid
    // position is also the start of the next cluster (recovering alignment).
    //
    // Since clusters are contiguous, start_of_next == end_of_current.
    //
    return c->byte_offset + c->byte_length;
  }

  size_t grapheme_index::
  prev_boundary (size_t off) const
  {
    if (!segmentation_ || off == 0)
      return 0;

    // We are looking for the nearest boundary strictly less than `off`.
    //
    const auto* c (segmentation_->find_at_byte (off));

    if (c == nullptr)
    {
      // If `off` is beyond the last byte (e.g., cursor at EOF), the previous
      // boundary is the start of the very last cluster.
      //
      if (!segmentation_->clusters.empty ())
        return segmentation_->clusters.back ().byte_offset;
      return 0;
    }

    // Case 1: We are inside a cluster. Previous boundary is the start of
    // *this* cluster.
    //
    if (off > c->byte_offset)
      return c->byte_offset;

    // Case 2: We are at the start of a cluster. Previous boundary is the
    // start of the *previous* cluster.
    //
    if (c->index > 0)
      return segmentation_->clusters[c->index - 1].byte_offset;

    // Case 3: We are at the start of the first cluster (offset 0).
    //
    return 0;
  }

  bool grapheme_index::
  is_boundary (size_t off) const
  {
    if (!segmentation_)
      return true; // Empty string has boundary at 0.

    if (off == 0 || off == segmentation_->total_bytes)
      return true;

    const auto* c (segmentation_->find_at_byte (off));

    if (c == nullptr)
      return false;

    // It's a boundary only if it matches the start of the cluster exactly.
    //
    return off == c->byte_offset;
  }

  // Edit Context
  //

  bool grapheme_edit_context::
  apply (grapheme_index& idx, string_view s)
  {
    // The Dream vs. Reality.
    //
    // The dream is that we take the edit range, find the affected graphemes,
    // and patch the index vector in-place.
    //
    // The reality is that implementing a robust incremental update for Unicode
    // segmentation is a nightmare of edge cases. For now, we accept the
    // performance hit of full recomputation to guarantee correctness.
    //
    idx.update (s);
    return true;
  }

  grapheme_range::
  grapheme_range (const grapheme_segmentation& s) noexcept
    : s_ (&s)
  {
  }

  grapheme_iterator grapheme_range::
  begin () const noexcept
  {
    return s_->clusters.begin ();
  }

  grapheme_iterator grapheme_range::
  end () const noexcept
  {
    return s_->clusters.end ();
  }

  size_t grapheme_range::
  size () const noexcept
  {
    return s_->size ();
  }

  bool grapheme_range::
  empty () const noexcept
  {
    return s_->empty ();
  }
}
