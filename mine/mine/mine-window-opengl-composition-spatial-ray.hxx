#pragma once

#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>

#include <optional>
#include <algorithm>

#include <mine/mine-types.hxx>

namespace mine
{
  // 2D ray.
  //
  struct ray2
  {
    vec2 o; // Origin.
    vec2 d; // Direction.

    [[nodiscard]] vec2
    at (float t) const
    {
      return o + d * t;
    }
  };

  // 3D ray.
  //
  struct ray3
  {
    vec3 o;
    vec3 d;

    [[nodiscard]] vec3
    at (float t) const
    {
      return o + d * t;
    }
  };

  // Return the closest intersection distance between ray r and rectangle b.
  // If there is no intersection, return an empty optional.
  //
  [[nodiscard]] inline std::optional<float>
  intersect (const ray2& r, const rect& b)
  {
    float tn (0.0f);  // near intersection
    float tx (1e10f); // far intersection

    // Check the X slab.
    //
    if (r.d.x != 0.0f)
    {
      // Cache the inverse direction to avoid multiple divisions.
      //
      float i (1.0f / r.d.x);

      float t0 ((b.x - r.o.x) * i);
      float t1 ((b.right () - r.o.x) * i);

      if (t0 > t1)
        std::swap (t0, t1);

      tn = std::max (tn, t0);
      tx = std::min (tx, t1);
    }
    else if (r.o.x < b.x || r.o.x > b.right ())
    {
      // The ray is parallel to the X axis and originates outside the slab.
      //
      return std::nullopt;
    }

    // Check the Y slab.
    //
    if (r.d.y != 0.0f)
    {
      float i (1.0f / r.d.y);

      float t0 ((b.y - r.o.y) * i);
      float t1 ((b.bottom () - r.o.y) * i);

      if (t0 > t1)
        std::swap (t0, t1);

      tn = std::max (tn, t0);
      tx = std::min (tx, t1);
    }
    else if (r.o.y < b.y || r.o.y > b.bottom ())
    {
      // Parallel to the Y axis and outside.
      //
      return std::nullopt;
    }

    // If the far intersection is closer than the near intersection, the slabs
    // don't overlap and the ray missed the rectangle.
    //
    if (tx < tn)
      return std::nullopt;

    return tn;
  }
}
