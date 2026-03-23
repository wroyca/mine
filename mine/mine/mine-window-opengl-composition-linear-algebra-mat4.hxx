#pragma once

#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>

namespace mine
{
  // A 4x4 matrix stored in column-major order.
  //
  // We keep it column-major so we can shovel it directly into OpenGL
  // uniform functions without incurring any transposition overhead.
  //
  struct mat4
  {
    constexpr mat4 () = default;

    float m[16]
    {
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    constexpr float&
    operator () (std::size_t r, std::size_t c)
    {
      return m[c * 4 + r];
    }

    constexpr float
    operator () (std::size_t r, std::size_t c) const
    {
      return m[c * 4 + r];
    }

    const float*
    data () const
    {
      return m;
    }

    float*
    data ()
    {
      return m;
    }

    static mat4
    identity ()
    {
      return mat4 {};
    }

    static mat4
    ortho (float l, float r, float b, float t, float n = -1.0f, float f = 1.0f)
    {
      mat4 x;

      // Clear out the default identity initialization.
      //
      // It's probably redundant, but we prefer the safety of an
      // always-initialized default state over a garbage state.
      //
      for (int i (0); i < 16; ++i)
        x.m[i] = 0.0f;

      x (0, 0) =  2.0f / (r - l);
      x (1, 1) =  2.0f / (t - b);
      x (2, 2) = -2.0f / (f - n);

      x (0, 3) = -(r + l) / (r - l);
      x (1, 3) = -(t + b) / (t - b);
      x (2, 3) = -(f + n) / (f - n);
      x (3, 3) =  1.0f;

      return x;
    }

    // Produce a translation matrix.
    //
    static mat4
    translate (float x, float y, float z = 0.0f)
    {
      mat4 r;

      // We start with an identity matrix, so we only need to plug
      // the translation values right into the last column.
      //
      r (0, 3) = x;
      r (1, 3) = y;
      r (2, 3) = z;

      return r;
    }

    // Produce a scale matrix.
    //
    static mat4
    scale (float x, float y, float z = 1.0f)
    {
      mat4 r;

      // Again, clear the default identity state.
      //
      for (int i (0); i < 16; ++i)
        r.m[i] = 0.0f;

      r (0, 0) = x;
      r (1, 1) = y;
      r (2, 2) = z;
      r (3, 3) = 1.0f;

      return r;
    }
  };

  // Matrix-matrix multiplication.
  //
  [[nodiscard]] inline mat4
  operator * (const mat4& a, const mat4& b)
  {
    mat4 x;

    for (std::size_t i (0); i < 16; ++i)
      x.m[i] = 0.0f;

    // Standard naive matrix multiplication.
    //
    // We could unroll this or use SIMD intrinsics, but for our current
    // use-case the compiler does a decent enough job optimizing it.
    //
    for (std::size_t c (0); c < 4; ++c)
      for (std::size_t r (0); r < 4; ++r)
        for (std::size_t k (0); k < 4; ++k)
          x (r, c) += a (r, k) * b (k, c);

    return x;
  }

  // Matrix-vector multiplication.
  //
  [[nodiscard]] inline vec4
  operator * (const mat4& m, const vec4& v)
  {
    return {
      m (0, 0) * v.x + m (0, 1) * v.y + m (0, 2) * v.z + m (0, 3) * v.w,
      m (1, 0) * v.x + m (1, 1) * v.y + m (1, 2) * v.z + m (1, 3) * v.w,
      m (2, 0) * v.x + m (2, 1) * v.y + m (2, 2) * v.z + m (2, 3) * v.w,
      m (3, 0) * v.x + m (3, 1) * v.y + m (3, 2) * v.z + m (3, 3) * v.w
    };
  }
}
