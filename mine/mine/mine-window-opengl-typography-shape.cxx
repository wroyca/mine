#include <mine/mine-window-opengl-typography-shape.hxx>

#include <utility>

using namespace std;

namespace mine
{
  text_blob::
  text_blob (vector<glyph_info> g, float w, float h, float a, float d)
    : g_ (move (g)),
      w_ (w),
      h_ (h),
      a_ (a),
      d_ (d)
  {
  }

  text_blob_builder::
  text_blob_builder ()
  {
  }

  void text_blob_builder::
  add_glyph (const glyph_info& g)
  {
    glyph_info i (g);

    // Calculate absolute positions relative to the blob's origin. The X
    // position is advanced by the current cursor. For Y, the baseline is
    // established by the ascender, so we subtract the Y bearing to drop the
    // glyph into its correct vertical placement.
    //
    i.x = cx_ + i.bx;
    i.y = a_ - i.by;

    cx_ += i.adv;

    g_.push_back (move (i));
  }

  void text_blob_builder::
  set_metrics (float a, float d)
  {
    // Store the ascender and descender directly. We will need them later to
    // calculate the total bounding box height once all the glyphs have been
    // processed and the blob is ready to be built.
    //
    a_ = a;
    d_ = d;
  }

  text_blob text_blob_builder::
  build ()
  {
    // The total height of the blob is simply the distance from the very top of
    // the ascender to the bottom of the descender.
    //
    float h (a_ - d_);

    // Move the accumulated glyphs into the text blob to avoid unnecessary
    // allocations. Note that the final cursor X position effectively gives us
    // the total typographic width.
    //
    return text_blob (move (g_), cx_, h, a_, d_);
  }

  void text_blob_builder::
  reset ()
  {
    // Clear the accumulated glyphs but keep the underlying vector capacity
    // intact to prevent re-allocations during subsequent blob constructions.
    // Naturally, we also reset the drawing cursor back to the origin.
    //
    g_.clear ();
    cx_ = 0.0f;
  }
}
