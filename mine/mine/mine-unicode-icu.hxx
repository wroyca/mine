#pragma once

#include <memory>
#include <string_view>

#include <unicode/brkiter.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

namespace mine
{
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
    icu_break_iterator (type t,
                        const icu::Locale& l = icu::Locale::getDefault ());

    // Move-only.
    //
    // ICU iterators are heavy objects with internal state caches. Copying them
    // is expensive and rarely what you want.
    //
    icu_break_iterator (const icu_break_iterator&) = delete;
    icu_break_iterator& operator= (const icu_break_iterator&) = delete;

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
    set_text (const icu::UnicodeString& s);

    // Navigation
    //
    // These wrap the raw ICU calls, returning `BreakIterator::DONE` (-1) when
    // boundaries are exhausted.
    //

    int32_t first ();
    int32_t last ();
    int32_t next ();
    int32_t previous ();
    int32_t current () const;

    // Smart navigation.
    //
    // `following(n)` finds the first boundary strictly after `n`.
    // `preceding(n)` finds the first boundary strictly before `n`.
    //
    int32_t following (int32_t off);
    int32_t preceding (int32_t off);

    // Is the given offset a valid boundary?
    //
    bool
    is_boundary (int32_t off);

    // Escape hatch for raw API access.
    //
    icu::BreakIterator&       get ();
    const icu::BreakIterator& get () const;

    bool
    valid () const
    {
      return iter_ != nullptr;
    }

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
  utf16_offset_to_utf8 (const icu::UnicodeString& u,
                        std::string_view s,
                        int32_t u_off);

  // Map a UTF-8 byte offset to a UTF-16 offset.
  //
  // Complexity: O(N) relative to the offset.
  //
  int32_t
  utf8_offset_to_utf16 (const icu::UnicodeString& u,
                        std::string_view s,
                        std::size_t b_off);
}
