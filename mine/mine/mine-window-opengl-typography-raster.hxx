#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <string_view>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace mine
{
  // Glyph bitmap data.
  //
  // We keep the raw 8-bit coverage data here before moving it to the
  // texture atlas.
  //
  struct glyph_bitmap
  {
    std::uint32_t cp {0};
    std::uint32_t id {0};

    std::uint32_t w  {0};
    std::uint32_t h  {0};

    float bx  {0.0f};
    float by  {0.0f};
    float adv {0.0f};

    std::vector<std::uint8_t> data;
  };

  // FreeType rasterizer.
  //
  // Converts TrueType/OpenType vector outlines into raw bitmap coverage data.
  //
  class ft_rasterizer
  {
  public:
    ft_rasterizer ();
    ~ft_rasterizer ();

    ft_rasterizer (const ft_rasterizer&) = delete;
    ft_rasterizer& operator= (const ft_rasterizer&) = delete;

    ft_rasterizer (ft_rasterizer&&) noexcept;
    ft_rasterizer& operator= (ft_rasterizer&&) noexcept;

    // Load a font from path p with pixel size s. Return false if either the
    // library init or face loading fails.
    //
    bool
    load (std::string_view p, std::uint32_t s);

    // Rasterize a single codepoint. Return nullopt if the glyph is missing or
    // cannot be rendered.
    //
    std::optional<glyph_bitmap>
    rasterize (std::uint32_t cp);

    // Font metrics.
    //
    float ascender () const { return asc_; }
    float descender () const { return desc_; }
    float line_height () const { return lh_; }

  private:
    FT_Library lib_  {nullptr};
    FT_Face    face_ {nullptr};

    std::uint32_t sz_   {14};
    float         asc_  {0.0f};
    float         desc_ {0.0f};
    float         lh_   {0.0f};
  };
}
