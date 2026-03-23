#include <mine/mine-window-opengl-typography-raster.hxx>
#include <mine/mine-assert.hxx>

#include <string>
#include <cstring>
#include <iostream>

using namespace std;

namespace mine
{
  ft_rasterizer::
  ft_rasterizer ()
  {
    // We initialize the FreeType library here. Bail out if it fails, as we
    // really can't proceed without the underlying font engine.
    //
    FT_Error e (FT_Init_FreeType (&lib_));
    MINE_INVARIANT (e == 0);
  }

  ft_rasterizer::
  ~ft_rasterizer ()
  {
    // Clean up the face first, then the library. The order matters since the
    // face depends on the library's internal state.
    //
    if (face_)
      FT_Done_Face (face_);

    if (lib_)
      FT_Done_FreeType (lib_);
  }

  ft_rasterizer::
  ft_rasterizer (ft_rasterizer&& x) noexcept
    : lib_ (x.lib_),
      face_ (x.face_),
      sz_ (x.sz_),
      asc_ (x.asc_),
      desc_ (x.desc_),
      lh_ (x.lh_)
  {
    // Leave the moved-from object in a valid, empty state so the destructor
    // doesn't try to free resources we just stole.
    //
    x.lib_ = nullptr;
    x.face_ = nullptr;
  }

  ft_rasterizer& ft_rasterizer::
  operator= (ft_rasterizer&& x) noexcept
  {
    if (this != &x)
    {
      this->~ft_rasterizer ();
      new (this) ft_rasterizer (std::move (x));
    }
    return *this;
  }

  bool ft_rasterizer::
  load (string_view p, uint32_t s)
  {
    // If we already have a face loaded, get rid of it. We only support one
    // active font face per rasterizer instance.
    //
    if (face_)
    {
      FT_Done_Face (face_);
      face_ = nullptr;
    }

    // FreeType expects a null-terminated C string. We create a temporary string
    // here to guarantee we have the null terminator.
    //
    FT_Error e (FT_New_Face (lib_, string (p).c_str (), 0, &face_));

    if (e != 0)
      return false;

    sz_ = s;
    FT_Set_Pixel_Sizes (face_, 0, s);

    // Convert the typographic metrics from 1/64th of a pixel to standard
    // pixels. FreeType uses these fractional units internally for precision.
    //
    asc_  = static_cast<float> (face_->size->metrics.ascender) / 64.0f;
    desc_ = static_cast<float> (face_->size->metrics.descender) / 64.0f;
    lh_   = static_cast<float> (face_->size->metrics.height) / 64.0f;

    return true;
  }

  optional<glyph_bitmap> ft_rasterizer::
  rasterize (uint32_t c)
  {
    if (!face_)
      return nullopt;

    // Ask FreeType for the glyph index corresponding to this codepoint.
    //
    FT_UInt i (FT_Get_Char_Index (face_, c));

    // Index 0 represents the "missing glyph". If our codepoint wasn't 0 to
    // begin with, this means the font doesn't support this character.
    //
    if (i == 0 && c != 0)
      return nullopt;

    FT_Error e (FT_Load_Glyph (face_, i, FT_LOAD_RENDER));

    if (e != 0)
      return nullopt;

    FT_GlyphSlot gs (face_->glyph);
    FT_Bitmap& b (gs->bitmap);

    glyph_bitmap r;
    r.cp  = c;
    r.id  = i;
    r.w   = b.width;
    r.h   = b.rows;
    r.bx  = static_cast<float> (gs->bitmap_left);
    r.by  = static_cast<float> (gs->bitmap_top);
    r.adv = static_cast<float> (gs->advance.x) / 64.0f;

    if (b.buffer && b.width > 0 && b.rows > 0)
    {
      size_t z (static_cast<size_t> (b.width) * b.rows);
      r.data.resize (z);

      // Handle pitch mismatch. Sometimes FreeType pads the rows for memory
      // alignment. If the pitch matches the width, we can safely perform a
      // single contiguous block copy.
      //
      if (b.pitch == static_cast<int> (b.width))
      {
        memcpy (r.data.data (), b.buffer, z);
      }
      else
      {
        // We have padding. Copy row by row to strip it out and pack the bitmap
        // data tightly.
        //
        for (unsigned int y (0); y < b.rows; ++y)
        {
          memcpy (r.data.data () + y * b.width,
                  b.buffer + y * b.pitch,
                  b.width);
        }
      }
    }

    return r;
  }
}
