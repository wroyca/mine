#include <mine/mine-unicode-icu.hxx>
#include <mine/mine-unicode-assert.hxx>

#include <unicode/brkiter.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>
#include <unicode/stringpiece.h>

#include <cassert>
#include <memory>
#include <string_view>

using namespace std;

namespace mine
{
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

  icu::BreakIterator& icu_break_iterator::
  get ()
  {
    assert (iter_ != nullptr);
    return *iter_;
  }

  const icu::BreakIterator& icu_break_iterator::
  get () const
  {
    assert (iter_ != nullptr);
    return *iter_;
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
}
