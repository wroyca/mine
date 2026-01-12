#pragma once

#include <cstddef>
#include <iterator>
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
  // This class assumes you have already paid the cost of segmentation via
  // `grapheme_index::update()`. Iterating here is just pointer arithmetic
  // and array indexing, making it safe to use in tight rendering loops (60fps).
  //
  class grapheme_iterator
  {
  public:
    // Since the underlying segmentation is a vector, we can support random
    // access. This lets us do things like `std::distance` in O(1).
    //
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = grapheme_cluster;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const grapheme_cluster*;
    using reference         = const grapheme_cluster&;

    grapheme_iterator () = default;

    grapheme_iterator (const grapheme_segmentation* s, std::size_t i)
      : s_ (s), i_ (i)
    {
    }

    //
    // Access
    //

    reference
    operator* () const
    {
      return s_->clusters[i_];
    }

    pointer
    operator-> () const
    {
      return &s_->clusters[i_];
    }

    reference
    operator[] (difference_type n) const
    {
      return s_->clusters[i_ + static_cast<std::size_t> (n)];
    }

    // Motion
    //

    grapheme_iterator&
    operator++ ()
    {
      ++i_;
      return *this;
    }

    grapheme_iterator
    operator++ (int)
    {
      grapheme_iterator r (*this);
      ++i_;
      return r;
    }

    grapheme_iterator&
    operator-- ()
    {
      --i_;
      return *this;
    }

    grapheme_iterator
    operator-- (int)
    {
      grapheme_iterator r (*this);
      --i_;
      return r;
    }

    grapheme_iterator&
    operator+= (difference_type n)
    {
      i_ += static_cast<std::size_t> (n);
      return *this;
    }

    grapheme_iterator&
    operator-= (difference_type n)
    {
      i_ -= static_cast<std::size_t> (n);
      return *this;
    }

    grapheme_iterator
    operator+ (difference_type n) const
    {
      return grapheme_iterator (s_, i_ + static_cast<std::size_t> (n));
    }

    grapheme_iterator
    operator- (difference_type n) const
    {
      return grapheme_iterator (s_, i_ - static_cast<std::size_t> (n));
    }

    difference_type
    operator- (const grapheme_iterator& x) const
    {
      return static_cast<difference_type> (i_) -
             static_cast<difference_type> (x.i_);
    }

    // Comparison
    //

    bool
    operator== (const grapheme_iterator& x) const
    {
      // Two iterators are equal if they point to the same segmentation
      // object and the same index within it.
      //
      return s_ == x.s_ && i_ == x.i_;
    }

    std::strong_ordering
    operator<=> (const grapheme_iterator& x) const
    {
      // Undefined Behavior check.
      //
      // Comparing iterators from different containers is logically invalid in
      // C++. There is no "ordering" between an iterator into String A and an
      // iterator into String B.
      //
      assert (s_ == x.s_);

      return i_ <=> x.i_;
    }

    // Raw index access (for when you need to jump back to integer math).
    //
    std::size_t
    index () const
    {
      return i_;
    }

  private:
    const grapheme_segmentation* s_ {nullptr};
    std::size_t i_ {0};
  };

  // Syntactic sugar for range-based for loops.
  //
  // Allows writing: `for (const auto& g : make_grapheme_range(seg))`.
  //
  class grapheme_range
  {
  public:
    explicit
    grapheme_range (const grapheme_segmentation& s)
      : s_ (&s)
    {
    }

    grapheme_iterator
    begin () const
    {
      return grapheme_iterator (s_, 0);
    }

    grapheme_iterator
    end () const
    {
      return grapheme_iterator (s_, s_->size ());
    }

    std::size_t
    size () const
    {
      return s_->size ();
    }

    bool
    empty () const
    {
      return s_->empty ();
    }

  private:
    const grapheme_segmentation* s_;
  };

  // Factory.
  //
  inline grapheme_range
  make_grapheme_range (const grapheme_segmentation& s)
  {
    return grapheme_range (s);
  }
}
