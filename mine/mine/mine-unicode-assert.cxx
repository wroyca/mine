#include <mine/mine-unicode-assert.hxx>
#include <mine/mine-core-buffer.hxx>
#include <mine/mine-core-cursor.hxx>
#include <mine/mine-unicode-grapheme.hxx>

#include <unicode/ustring.h>
#include <unicode/brkiter.h>
#include <unicode/utypes.h>

#include <cassert>
#include <memory>

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

    u_strFromUTF8 (nullptr,                // dest
                   0,                      // destCapacity
                   &n,                     // pDestLength
                   s.data (),              // src
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
    // Boundaries.
    //
    if (off == 0 || off == s.size ())
      return;

    assert (off <= s.size () && "Byte offset out of range");

    UErrorCode e (U_ZERO_ERROR);

    // The Impedance Mismatch.
    //
    // Our editor is UTF-8 (byte-oriented). ICU is UTF-16 (UChar-oriented). To
    // check if a byte offset is a boundary, we have to:
    //
    // 1. Convert the string to UTF-16.
    // 2. Map the byte offset to a UChar offset.
    // 3. Ask the BreakIterator.
    //
    // This is hideously slow, which is why this is strictly a debug check.
    //
    auto ustr (icu::UnicodeString::fromUTF8 (
                 icu::StringPiece (s.data (),
                                   static_cast<int32_t> (s.size ()))));

    unique_ptr<icu::BreakIterator> bi (
      icu::BreakIterator::createCharacterInstance (
        icu::Locale::getDefault (), e));

    assert (U_SUCCESS (e) && "Failed to create ICU break iterator");

    bi->setText (ustr);

    // Walk the string to translate offsets.
    //
    // We have to iterate code points because a single code point might be 3
    // bytes in UTF-8 but 1 unit in UTF-16, or 4 bytes in UTF-8 and 2 units
    // (surrogate pair) in UTF-16.
    //
    int32_t u16_off (0);
    size_t u8_pos (0);

    while (u8_pos < off && u16_off < ustr.length ())
    {
      UChar32 c;
      U16_NEXT (ustr.getBuffer (), u16_off, ustr.length (), c);
      u8_pos += U8_LENGTH (c);
    }

    // If we didn't land exactly on the offset, we jumped into the middle of
    // a multi-byte sequence.
    //
    assert (u8_pos == off && "Byte offset splits a UTF-8 code point");

    // Finally, check the grapheme break logic.
    //
    assert (bi->isBoundary (u16_off) &&
            "Byte offset splits a grapheme cluster");
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
      // We purposefully crash here because if ICU is failing, our assumptions
      // about text processing are void.
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
    // insertions).
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
