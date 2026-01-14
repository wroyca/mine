#include <mine/mine-unicode-assert.hxx>

#include <cassert>
#include <memory>

#include <unicode/ubrk.h>
#include <unicode/utext.h>
#include <unicode/ustring.h>

#include <mine/mine-core-buffer.hxx>
#include <mine/mine-core-cursor.hxx>
#include <mine/mine-unicode-grapheme.hxx>

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
}
