#pragma once

namespace mine
{
  template <typename T>
  struct rk4_state
  {
    T p; // Position.
    T v; // Velocity.

    rk4_state () = default;

    rk4_state (const T& p_, const T& v_)
      : p (p_),
        v (v_)
    {
    }
  };

  template <typename T>
  struct rk4_deriv
  {
    T x; // velocity (dx/dt)
    T v; // acceleration (dv/dt)

    rk4_deriv () = default;

    rk4_deriv (const T& x_, const T& v_)
      : x (x_),
        v (v_)
    {
    }
  };

  template <typename T, typename F>
  class rk4_integrator
  {
  public:
    explicit
    rk4_integrator (F a)
      : a_ (a)
    {
    }

    void
    integrate (T& p, T& v, float t)
    {
      // Set up our initial state from the references.
      //
      rk4_state<T> s (p, v);

      // We need an empty derivative to kick off the first evaluation.
      //
      rk4_deriv<T> d0 (T (), T ());

      // Evaluate the four Runge-Kutta stages. k1 is the slope at the start.
      // k2 and k3 are evaluated at the midpoint, and k4 at the end.
      //
      rk4_deriv<T> k1 (eval (s, 0.0f, d0));
      rk4_deriv<T> k2 (eval (s, t * 0.5f, k1));
      rk4_deriv<T> k3 (eval (s, t * 0.5f, k2));
      rk4_deriv<T> k4 (eval (s, t, k3));

      // Calculate the final weighted sum of our sampled slopes.
      //
      // We double the weight of the midpoint samples (k2 and k3) to cancel out
      // lower-order error terms, which is standard RK4.
      //
      T dx ((k1.x + (k2.x + k3.x) * 2.0f + k4.x) * (1.0f / 6.0f));
      T dv ((k1.v + (k2.v + k3.v) * 2.0f + k4.v) * (1.0f / 6.0f));

      // Finally advance the actual position and velocity.
      //
      p = (p + dx * t);
      v = (v + dv * t);
    }

  private:
    F a_; // Acceleration functor.

    rk4_deriv<T>
    eval (const rk4_state<T>& i, float t, const rk4_deriv<T>& d)
    {
      // Step the initial state i forward by t using the projected derivative d.
      //
      T p (i.p + d.x * t);
      T v (i.v + d.v * t);

      // Then evaluate and return the new derivative (acceleration) at this
      // intermediate state.
      //
      return rk4_deriv<T> (v, a_ (p, v));
    }
  };
}
