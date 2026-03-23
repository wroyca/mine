#pragma once

#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>
#include <mine/mine-window-opengl-composition-linear-algebra-mat4.hxx>

namespace mine
{
  // Unit quaternion for rotations.
  //
  struct quat
  {
    float w {1.0f};
    float x {0.0f};
    float y {0.0f};
    float z {0.0f};

    constexpr quat () = default;
    constexpr quat (float w, float x, float y, float z)
      : w (w), x (x), y (y), z (z) {}

    static constexpr quat
    identity ()
    {
      return quat {};
    }

    static quat
    from_axis_angle (const vec3& a, float r)
    {
      // Calculate the half-angle and its sine. We also make sure to normalize
      // the axis vector here so we don't end up with a skewed rotation.
      //
      float h (r * 0.5f);
      float s (std::sin (h));
      vec3 n (a.normalized ());

      return {std::cos (h), n.x * s, n.y * s, n.z * s};
    }

    static quat
    from_z_rotation (float r)
    {
      return from_axis_angle ({0.0f, 0.0f, 1.0f}, r);
    }

    [[nodiscard]] float
    length_sq () const
    {
      return w * w + x * x + y * y + z * z;
    }

    [[nodiscard]] float
    length () const
    {
      return std::sqrt (length_sq ());
    }

    [[nodiscard]] quat
    normalized () const
    {
      float l (length ());

      // Fall back to the identity quaternion. If the length is zero, we can't
      // really do much else without hitting a division by zero.
      //
      if (l > 0.0f)
        return {w / l, x / l, y / l, z / l};

      return identity ();
    }

    [[nodiscard]] mat4
    to_matrix () const
    {
      mat4 m;

      // Pre-calculate products to avoid redundant multiplications down the
      // line.
      //
      float xx (x * x), yy (y * y), zz (z * z);
      float xy (x * y), xz (x * z), yz (y * z);
      float wx (w * x), wy (w * y), wz (w * z);

      m (0, 0) = 1.0f - 2.0f * (yy + zz);
      m (0, 1) = 2.0f * (xy - wz);
      m (0, 2) = 2.0f * (xz + wy);

      m (1, 0) = 2.0f * (xy + wz);
      m (1, 1) = 1.0f - 2.0f * (xx + zz);
      m (1, 2) = 2.0f * (yz - wx);

      m (2, 0) = 2.0f * (xz - wy);
      m (2, 1) = 2.0f * (yz + wx);
      m (2, 2) = 1.0f - 2.0f * (xx + yy);

      return m;
    }
  };

  // Hamilton product of two quaternions.
  //
  [[nodiscard]] inline quat
  operator * (const quat& a, const quat& b)
  {
    return {
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
  }

  // Spherical linear interpolation.
  //
  [[nodiscard]] inline quat
  slerp (const quat& a, const quat& b, float t)
  {
    // Compute the cosine of the angle between the two vectors.
    //
    float d (a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);

    quat c (b);

    // If the dot product is negative, the quaternions have opposite handedness.
    // We flip one to ensure we take the shortest path along the sphere.
    //
    if (d < 0.0f)
    {
      d = -d;
      c = {-b.w, -b.x, -b.y, -b.z};
    }

    // If the inputs are too close, fall back to linear interpolation.
    // Otherwise, we risk a division by zero when calculating the spherical
    // multipliers.
    //
    if (d > 0.9995f)
    {
      return quat {
        a.w + t * (c.w - a.w),
        a.x + t * (c.x - a.x),
        a.y + t * (c.y - a.y),
        a.z + t * (c.z - a.z)
      }.normalized ();
    }

    // Calculate the spherical interpolation multipliers.
    //
    float th (std::acos (d));
    float s  (std::sin (th));
    float w1 (std::sin ((1.0f - t) * th) / s);
    float w2 (std::sin (t * th) / s);

    return {
      w1 * a.w + w2 * c.w,
      w1 * a.x + w2 * c.x,
      w1 * a.y + w2 * c.y,
      w1 * a.z + w2 * c.z
    };
  }
}
