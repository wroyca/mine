#pragma once

#include <vector>
#include <cstdint>

#include <mine/mine-types.hxx>

namespace mine
{
  // Placed glyph information. We keep the bare minimum required to actually
  // render the quad. Note that coordinates are relative to the blob origin.
  //
  struct glyph_info
  {
    std::uint32_t cp {0};
    std::uint32_t id {0};

    float x {0.0f};
    float y {0.0f};
    float w {0.0f};
    float h {0.0f};

    uv_rect uv;

    float adv {0.0f};
    float bx  {0.0f};
    float by  {0.0f};
  };

  // Shaped text blob.
  //
  // Once built, it is completely immutable. We can query it for hit-testing or
  // simply iterate over the glyphs to produce the actual rendering geometry.
  //
  class text_blob
  {
  public:
    text_blob () = default;

    text_blob (std::vector<glyph_info> g, float w, float h, float a, float d);

    const std::vector<glyph_info>&
    glyphs () const
    {
      return g_;
    }

    float
    width () const
    {
      return w_;
    }

    float
    height () const
    {
      return h_;
    }

    float
    ascender () const
    {
      return a_;
    }

    float
    descender () const
    {
      return d_;
    }

  private:
    std::vector<glyph_info> g_;

    float w_ {0.0f};
    float h_ {0.0f};
    float a_ {0.0f};
    float d_ {0.0f};
  };

  // Builder for assembling a text blob. We accumulate glyphs one by one and
  // keep track of the advancing cursor and overall font metrics.
  //
  class text_blob_builder
  {
  public:
    text_blob_builder ();

    void
    add_glyph (const glyph_info& g);

    void
    set_metrics (float asc, float desc);

    text_blob
    build ();

    void
    reset ();

  private:
    std::vector<glyph_info> g_;
    float a_  {0.0f};
    float d_  {0.0f};
    float cx_ {0.0f}; // Cursor X.
  };
}
