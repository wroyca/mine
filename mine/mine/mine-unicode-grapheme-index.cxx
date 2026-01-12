#include <mine/mine-unicode-grapheme-index.hxx>
#include <mine/mine-unicode-grapheme.hxx>
#include <mine/mine-unicode-assert.hxx>

#include <cassert>
#include <memory>

using namespace std;

namespace mine
{
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

    // We are looking for the nearest boundary strictly greater than `off`.
    //
    const auto* c (segmentation_->find_at_byte (off));

    if (c == nullptr)
      return segmentation_->total_bytes;

    // Case 1: We are inside a cluster (e.g., pointing at the 2nd byte of a
    // 3-byte character). The next boundary is the end of *this* cluster.
    //
    if (off > c->byte_offset)
      return c->byte_offset + c->byte_length;

    // Case 2: We are at the start of a cluster. The next boundary is the
    // start of the *next* cluster.
    //
    if (c->index + 1 < segmentation_->size ())
      return segmentation_->clusters[c->index + 1].byte_offset;

    // Case 3: We are at the start of the last cluster. Next boundary is EOF.
    //
    return segmentation_->total_bytes;
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
}
