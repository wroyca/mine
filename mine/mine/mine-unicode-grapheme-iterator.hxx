#pragma once

#include <cstddef>
#include <iterator>
#include <compare> // C++20: <=>

#include <immer/vector.hpp>

#include <mine/mine-unicode-grapheme.hxx>

namespace mine
{
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
    grapheme_range (const grapheme_segmentation& s) noexcept
      : s_ (&s)
    {
    }

    [[nodiscard]] grapheme_iterator
    begin () const noexcept
    {
      return s_->clusters.begin ();
    }

    [[nodiscard]] grapheme_iterator
    end () const noexcept
    {
      return s_->clusters.end ();
    }

    [[nodiscard]] std::size_t
    size () const noexcept
    {
      return s_->size ();
    }

    [[nodiscard]] bool
    empty () const noexcept
    {
      return s_->empty ();
    }

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
