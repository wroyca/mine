#include <mine/mine-window-opengl-typography-atlas.hxx>

#include <algorithm>
#include <cstring>

using namespace std;

namespace mine
{
  // bin_packer
  //

  bin_packer::
  bin_packer (uint32_t w, uint32_t h)
    : w_ (w),
      h_ (h)
  {
    free_.push_back ({0, 0, w, h});
  }

  optional<pack_res> bin_packer::
  pack (uint32_t w, uint32_t h)
  {
    // We use the best short side fit heuristic here. The idea is to find a free
    // rectangle that minimizes the remaining space along the short side after
    // placing our new rect. This keeps the leftover areas as square and usable
    // as possible.
    //
    size_t b (free_.size ());
    uint32_t bs (UINT32_MAX);
    uint32_t bl (UINT32_MAX);

    for (size_t i (0); i < free_.size (); ++i)
    {
      const auto& r (free_[i]);

      if (r.w >= w && r.h >= h)
      {
        uint32_t rh (r.h - h);
        uint32_t rw (r.w - w);

        uint32_t ss (min (rw, rh));
        uint32_t ls (max (rw, rh));

        if (ss < bs || (ss == bs && ls < bl))
        {
          b = i;
          bs = ss;
          bl = ls;
        }
      }
    }

    // Bail out if we couldn't find a spot.
    //
    if (b == free_.size ())
      return nullopt;

    pack_rect r (free_[b]);
    pack_res res {r.x, r.y};

    split (b, w, h);
    prune ();

    return res;
  }

  void bin_packer::
  reset ()
  {
    // Simply wipe the list and push the full initial area back as our single
    // free block.
    //
    free_.clear ();
    free_.push_back ({0, 0, w_, h_});
  }

  void bin_packer::
  split (size_t i, uint32_t w, uint32_t h)
  {
    // Cut the rectangle at index i into two smaller free rectangles
    // representing the remainder of the space.
    //
    pack_rect r (free_[i]);
    free_.erase (free_.begin () + static_cast<ptrdiff_t> (i));

    if (r.w > w)
      free_.push_back ({r.x + w, r.y, r.w - w, h});

    if (r.h > h)
      free_.push_back ({r.x, r.y + h, r.w, r.h - h});
  }

  void bin_packer::
  prune ()
  {
    // Weed out any free rectangles that are completely contained within
    // another. This prevents the list from growing unnecessarily and helps keep
    // the packing routine fast.
    //
    for (size_t i (0); i < free_.size (); ++i)
    {
      for (size_t j (i + 1); j < free_.size ();)
      {
        if (contains (free_[i], free_[j]))
        {
          free_.erase (free_.begin () + static_cast<ptrdiff_t> (j));
        }
        else if (contains (free_[j], free_[i]))
        {
          free_.erase (free_.begin () + static_cast<ptrdiff_t> (i));
          --i;
          break;
        }
        else
        {
          ++j;
        }
      }
    }
  }

  bool bin_packer::
  contains (const pack_rect& a, const pack_rect& b) const
  {
    return b.x >= a.x && b.y >= a.y &&
           b.x + b.w <= a.x + a.w &&
           b.y + b.h <= a.y + a.h;
  }

  // texture_updater
  //

  texture_updater::
  texture_updater (uint32_t w, uint32_t h)
    : w_ (w),
      h_ (h),
      stage_ (w * h, 0)
  {
  }

  void texture_updater::
  write (uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t* d)
  {
    // Sanity check to prevent scribbling past our staging buffer.
    //
    if (x + w > w_ || y + h > h_)
      return;

    for (uint32_t r (0); r < h; ++r)
    {
      size_t dst ((y + r) * w_ + x);
      size_t src (r * w);
      memcpy (stage_.data () + dst, d + src, w);
    }

    // Expand the dirty region to encompass this new write. If this is the first
    // write in the cycle, initialize the region instead.
    //
    if (dirty_)
    {
      uint32_t rx (dr_.x + dr_.w);
      uint32_t by (dr_.y + dr_.h);

      dr_.x = min (dr_.x, x);
      dr_.y = min (dr_.y, y);
      dr_.w = max (rx, x + w) - dr_.x;
      dr_.h = max (by, y + h) - dr_.y;
    }
    else
    {
      dr_ = {x, y, w, h};
      dirty_ = true;
    }
  }

  void texture_updater::
  clear ()
  {
    fill (stage_.begin (), stage_.end (), static_cast<uint8_t> (0));
    dr_ = {0, 0, w_, h_};
    dirty_ = true;
  }

  dirty_region texture_updater::
  flush ()
  {
    // If nothing was touched since the last flush, just return an empty region
    // to save the caller some work.
    //
    if (!dirty_)
      return {{0, 0, 0, 0}, {}};

    dirty_region r;
    r.r = dr_;

    size_t sz (static_cast<size_t> (dr_.w) * dr_.h);
    r.data.resize (sz);

    // Extract the dirty bounding box from the staging buffer so we only upload
    // exactly what changed to the GPU.
    //
    for (uint32_t i (0); i < dr_.h; ++i)
    {
      size_t src ((dr_.y + i) * w_ + dr_.x);
      size_t dst (i * dr_.w);
      memcpy (r.data.data () + dst, stage_.data () + src, dr_.w);
    }

    dirty_ = false;
    dr_ = {0, 0, 0, 0};

    return r;
  }
}
