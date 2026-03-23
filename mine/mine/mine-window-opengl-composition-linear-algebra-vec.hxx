#pragma once

#include <cmath>
#include <cstddef>

namespace mine
{
  // 2D vector for things like positions and UVs.
  //
  struct vec2
  {
    float x;
    float y;

    constexpr vec2 ()
      : x (0.0f),
        y (0.0f)
    {
    }

    constexpr vec2 (float s)
      : x (s),
        y (s)
    {
    }

    constexpr vec2 (float a, float b)
      : x (a),
        y (b)
    {
    }

    // Note that we rely on standard layout guarantees here to treat the struct
    // as an array. It avoids union shenanigans and is well-supported in
    // practice by major compilers.
    //
    constexpr float&
    operator [] (std::size_t i)
    {
      return (&x)[i];
    }

    constexpr float
    operator [] (std::size_t i) const
    {
      return (&x)[i];
    }

    constexpr vec2
    operator + (const vec2& v) const
    {
      return vec2 (x + v.x, y + v.y);
    }

    constexpr vec2
    operator - (const vec2& v) const
    {
      return vec2 (x - v.x, y - v.y);
    }

    constexpr vec2
    operator * (const vec2& v) const
    {
      return vec2 (x * v.x, y * v.y);
    }

    constexpr vec2
    operator / (const vec2& v) const
    {
      return vec2 (x / v.x, y / v.y);
    }

    constexpr vec2
    operator * (float s) const
    {
      return vec2 (x * s, y * s);
    }

    constexpr vec2
    operator / (float s) const
    {
      return vec2 (x / s, y / s);
    }

    constexpr vec2
    operator - () const
    {
      return vec2 (-x, -y);
    }

    constexpr vec2&
    operator += (const vec2& v)
    {
      x += v.x;
      y += v.y;
      return *this;
    }

    constexpr vec2&
    operator -= (const vec2& v)
    {
      x -= v.x;
      y -= v.y;
      return *this;
    }

    constexpr vec2&
    operator *= (float s)
    {
      x *= s;
      y *= s;
      return *this;
    }

    constexpr vec2&
    operator /= (float s)
    {
      x /= s;
      y /= s;
      return *this;
    }

    constexpr bool
    operator == (const vec2&) const = default;

    [[nodiscard]] float
    length_sq () const
    {
      return x * x + y * y;
    }

    [[nodiscard]] float
    length () const
    {
      return std::sqrt (length_sq ());
    }

    [[nodiscard]] vec2
    normalized () const
    {
      // Cache the length so we only compute the square root once. We fall back
      // to a zero vector to handle degenerate cases gracefully rather than
      // generating NaNs.
      //
      float l (length ());
      return l > 0.0f ? *this / l : vec2 ();
    }
  };

  constexpr vec2
  operator * (float s, const vec2& v)
  {
    return v * s;
  }

  [[nodiscard]] constexpr float
  dot (const vec2& a, const vec2& b)
  {
    return a.x * b.x + a.y * b.y;
  }

  [[nodiscard]] constexpr vec2
  lerp (const vec2& a, const vec2& b, float t)
  {
    return a + (b - a) * t;
  }

  // 3D vector.
  //
  struct vec3
  {
    float x;
    float y;
    float z;

    constexpr vec3 ()
      : x (0.0f),
        y (0.0f),
        z (0.0f)
    {
    }

    constexpr vec3 (float s)
      : x (s),
        y (s),
        z (s)
    {
    }

    constexpr vec3 (float a, float b, float c)
      : x (a),
        y (b),
        z (c)
    {
    }

    constexpr vec3 (const vec2& v, float c)
      : x (v.x),
        y (v.y),
        z (c)
    {
    }

    constexpr float&
    operator [] (std::size_t i)
    {
      return (&x)[i];
    }

    constexpr float
    operator [] (std::size_t i) const
    {
      return (&x)[i];
    }

    constexpr vec3
    operator + (const vec3& v) const
    {
      return vec3 (x + v.x, y + v.y, z + v.z);
    }

    constexpr vec3
    operator - (const vec3& v) const
    {
      return vec3 (x - v.x, y - v.y, z - v.z);
    }

    constexpr vec3
    operator * (const vec3& v) const
    {
      return vec3 (x * v.x, y * v.y, z * v.z);
    }

    constexpr vec3
    operator / (const vec3& v) const
    {
      return vec3 (x / v.x, y / v.y, z / v.z);
    }

    constexpr vec3
    operator * (float s) const
    {
      return vec3 (x * s, y * s, z * s);
    }

    constexpr vec3
    operator / (float s) const
    {
      return vec3 (x / s, y / s, z / s);
    }

    constexpr vec3
    operator - () const
    {
      return vec3 (-x, -y, -z);
    }

    constexpr vec3&
    operator += (const vec3& v)
    {
      x += v.x;
      y += v.y;
      z += v.z;
      return *this;
    }

    constexpr vec3&
    operator -= (const vec3& v)
    {
      x -= v.x;
      y -= v.y;
      z -= v.z;
      return *this;
    }

    constexpr vec3&
    operator *= (float s)
    {
      x *= s;
      y *= s;
      z *= s;
      return *this;
    }

    constexpr vec3&
    operator /= (float s)
    {
      x /= s;
      y /= s;
      z /= s;
      return *this;
    }

    constexpr bool
    operator == (const vec3&) const = default;

    [[nodiscard]] float
    length_sq () const
    {
      return x * x + y * y + z * z;
    }

    [[nodiscard]] float
    length () const
    {
      return std::sqrt (length_sq ());
    }

    [[nodiscard]] vec3
    normalized () const
    {
      float l (length ());
      return l > 0.0f ? *this / l : vec3 ();
    }

    // Swizzle helpers. We keep them minimal to avoid API bloat.
    //
    [[nodiscard]] vec2
    xy () const
    {
      return vec2 (x, y);
    }
  };

  constexpr vec3
  operator * (float s, const vec3& v)
  {
    return v * s;
  }

  [[nodiscard]] constexpr float
  dot (const vec3& a, const vec3& b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }

  [[nodiscard]] constexpr vec3
  cross (const vec3& a, const vec3& b)
  {
    return vec3 (a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x);
  }

  [[nodiscard]] constexpr vec3
  lerp (const vec3& a, const vec3& b, float t)
  {
    return a + (b - a) * t;
  }

  // 4D vector, mostly used for homogeneous coordinates or RGBA colors.
  //
  struct vec4
  {
    float x;
    float y;
    float z;
    float w;

    constexpr vec4 ()
      : x (0.0f),
        y (0.0f),
        z (0.0f),
        w (0.0f)
    {
    }

    constexpr vec4 (float s)
      : x (s),
        y (s),
        z (s),
        w (s)
    {
    }

    constexpr vec4 (float a, float b, float c, float d)
      : x (a),
        y (b),
        z (c),
        w (d)
    {
    }

    constexpr vec4 (const vec3& v, float d)
      : x (v.x),
        y (v.y),
        z (v.z),
        w (d)
    {
    }

    constexpr vec4 (const vec2& v, float c, float d)
      : x (v.x),
        y (v.y),
        z (c),
        w (d)
    {
    }

    constexpr float&
    operator [] (std::size_t i)
    {
      return (&x)[i];
    }

    constexpr float
    operator [] (std::size_t i) const
    {
      return (&x)[i];
    }

    constexpr vec4
    operator + (const vec4& v) const
    {
      return vec4 (x + v.x, y + v.y, z + v.z, w + v.w);
    }

    constexpr vec4
    operator - (const vec4& v) const
    {
      return vec4 (x - v.x, y - v.y, z - v.z, w - v.w);
    }

    constexpr vec4
    operator * (const vec4& v) const
    {
      return vec4 (x * v.x, y * v.y, z * v.z, w * v.w);
    }

    constexpr vec4
    operator / (const vec4& v) const
    {
      return vec4 (x / v.x, y / v.y, z / v.z, w / v.w);
    }

    constexpr vec4
    operator * (float s) const
    {
      return vec4 (x * s, y * s, z * s, w * s);
    }

    constexpr vec4
    operator / (float s) const
    {
      return vec4 (x / s, y / s, z / s, w / s);
    }

    constexpr vec4
    operator - () const
    {
      return vec4 (-x, -y, -z, -w);
    }

    constexpr bool
    operator == (const vec4&) const = default;

    [[nodiscard]] float
    length_sq () const
    {
      return x * x + y * y + z * z + w * w;
    }

    [[nodiscard]] float
    length () const
    {
      return std::sqrt (length_sq ());
    }

    [[nodiscard]] vec4
    normalized () const
    {
      float l (length ());
      return l > 0.0f ? *this / l : vec4 ();
    }

    [[nodiscard]] vec2
    xy () const
    {
      return vec2 (x, y);
    }

    [[nodiscard]] vec3
    xyz () const
    {
      return vec3 (x, y, z);
    }
  };

  constexpr vec4
  operator * (float s, const vec4& v)
  {
    return v * s;
  }

  [[nodiscard]] constexpr float
  dot (const vec4& a, const vec4& b)
  {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  }

  [[nodiscard]] constexpr vec4
  lerp (const vec4& a, const vec4& b, float t)
  {
    return a + (b - a) * t;
  }
}
