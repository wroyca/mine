#pragma once

// Semi-Implicit Euler Integration
//
// Unlike standard explicit Euler (which updates position, then velocity),
// this updates velocity first, then position using the *new* velocity.
//
// Why does this matter? It makes the integrator Symplectic, meaning it
// loosely conserves energy in oscillatory systems (like springs) rather than
// exploding.

namespace mine
{
template <typename T>
  struct semi_implicit_euler
  {
    static void
    integrate (T& p, T& v, const T& a, float d)
    {
      v = (v + a * d);
      p = (p + v * d);
    }
  };

  // Verlet Integration
  //
  // We generally use this when we want to enforce geometric constraints easily.
  // Notice that instead of storing the velocity directly, we derive it
  // implicitly from the previous position.
  //
  template <typename T>
  struct verlet
  {
    static void
    integrate (T& p, T& o, const T& a, float d)
    {
      T x (p);
      p = (p * 2.0f - o + a * (d * d));
      o = (x);
    }
  };
}
