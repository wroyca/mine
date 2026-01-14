#pragma once

#include <cstddef>
#include <iterator>
#include <compare> // C++20: <=>
#include <cassert>

#include <mine/mine-unicode-grapheme.hxx>

namespace mine
{
  // A lightweight cursor over pre-computed grapheme clusters.
  //
  // Architectural Note:
  // We decoupled the "discovery" of graphemes (ICU BreakIterator, which is
  // heavy and stateful) from the "traversal" (this iterator).
  //
  //
  class grapheme_iterator
  {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept  = std::contiguous_iterator_tag;
    using value_type        = grapheme_cluster;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const grapheme_cluster*;
    using reference         = const grapheme_cluster&;

    grapheme_iterator () = default;

    // We keep this constructor explicit to avoid accidental promotion of raw
    // pointers into iterators in unrelated contexts.
    //
    explicit
    grapheme_iterator (pointer p) noexcept
      : current_ (p)
    {
    }

    // Access
    //

    [[nodiscard]] reference
    operator* () const noexcept
    {
      return *current_;
    }

    [[nodiscard]] pointer
    operator-> () const noexcept
    {
      return current_;
    }

    [[nodiscard]] reference
    operator[] (difference_type n) const noexcept
    {
      return current_[n];
    }

    // Motion
    //

    grapheme_iterator&
    operator++ () noexcept
    {
      ++current_;
      return *this;
    }

    grapheme_iterator
    operator++ (int) noexcept
    {
      grapheme_iterator r (*this);
      ++current_;
      return r;
    }

    grapheme_iterator&
    operator-- () noexcept
    {
      --current_;
      return *this;
    }

    grapheme_iterator
    operator-- (int) noexcept
    {
      grapheme_iterator r (*this);
      --current_;
      return r;
    }

    grapheme_iterator&
    operator+= (difference_type n) noexcept
    {
      current_ += n;
      return *this;
    }

    grapheme_iterator&
    operator-= (difference_type n) noexcept
    {
      current_ -= n;
      return *this;
    }

    [[nodiscard]] grapheme_iterator
    operator+ (difference_type n) const noexcept
    {
      return grapheme_iterator (current_ + n);
    }

    [[nodiscard]] grapheme_iterator
    operator- (difference_type n) const noexcept
    {
      return grapheme_iterator (current_ - n);
    }

    [[nodiscard]] difference_type
    operator- (const grapheme_iterator& x) const noexcept
    {
      return current_ - x.current_;
    }

    // Comparison
    //

    bool
    operator== (const grapheme_iterator& x) const noexcept
    {
      return current_ == x.current_;
    }

    std::strong_ordering
    operator<=> (const grapheme_iterator& x) const noexcept
    {
      return current_ <=> x.current_;
    }

    [[nodiscard]] pointer
    base () const noexcept
    {
      return current_;
    }

  private:
    pointer current_ {nullptr};
  };

  // Allow symmetry: 1 + it.
  //
  [[nodiscard]] inline grapheme_iterator
  operator+ (grapheme_iterator::difference_type n, grapheme_iterator x) noexcept
  {
    return x + n;
  }

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
      // Assuming s_->clusters is a vector-like container that guarantees
      // contiguous storage. We take the address of the first element.
      //
      // If clusters is empty, data() usually returns a valid pointer (or
      // nullptr) such that data() == data() + size() is true.
      //
      return grapheme_iterator (s_->clusters.data ());
    }

    [[nodiscard]] grapheme_iterator
    end () const noexcept
    {
      return grapheme_iterator (s_->clusters.data () + s_->clusters.size ());
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
