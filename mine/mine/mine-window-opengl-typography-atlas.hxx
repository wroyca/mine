#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace mine
{
  // Pack result.
  //
  // Holds the top-left coordinates of the placed rectangle.
  //
  struct pack_res
  {
    std::uint32_t x {0};
    std::uint32_t y {0};
  };

  // Internal bin rectangle.
  //
  struct pack_rect
  {
    std::uint32_t x {0};
    std::uint32_t y {0};
    std::uint32_t w {0};
    std::uint32_t h {0};
  };

  // Bin packer.
  //
  // We manage the spatial allocation of the texture atlas here. The
  // implementation uses a simple free-rectangle split algorithm which is
  // usually good enough for typical glyph packing and keeps fragmentation
  // reasonably low.
  //
  class bin_packer
  {
  public:
    bin_packer (std::uint32_t w, std::uint32_t h);

    std::optional<pack_res>
    pack (std::uint32_t w, std::uint32_t h);

    void
    reset ();

    std::uint32_t
    width () const
    {
      return w_;
    }

    std::uint32_t
    height () const
    {
      return h_;
    }

  private:
    void
    split (std::size_t i, std::uint32_t w, std::uint32_t h);

    void
    prune ();

    bool
    contains (const pack_rect& a, const pack_rect& b) const;

  private:
    std::uint32_t w_;
    std::uint32_t h_;
    std::vector<pack_rect> free_;
  };

  // Dirty region for GPU uploads.
  //
  struct dirty_region
  {
    pack_rect r;
    std::vector<std::uint8_t> data;
  };

  // Texture updater.
  //
  // We accumulate small glyph writes into a staging buffer and track
  // the overall bounding box. This way we can flush everything to the
  // GPU in a single sub-image update instead of bottlenecking on
  // multiple tiny writes.
  //
  class texture_updater
  {
  public:
    texture_updater (std::uint32_t w, std::uint32_t h);

    void
    write (std::uint32_t x,
           std::uint32_t y,
           std::uint32_t w,
           std::uint32_t h,
           const std::uint8_t* d);

    void
    clear ();

    bool
    is_dirty () const
    {
      return dirty_;
    }

    dirty_region
    flush ();

  private:
    std::uint32_t w_;
    std::uint32_t h_;
    std::vector<std::uint8_t> stage_;

    bool dirty_ {false};
    pack_rect dr_; // Dirty rect.
  };
}
