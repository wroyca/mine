#pragma once

#include <mine/mine-contract.hxx>
#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>

#include <cmath>
#include <algorithm>

namespace mine
{
  template <typename T>
  class second_order_dynamics
  {
  public:
    second_order_dynamics ()
      : second_order_dynamics (T (), 1.0f, 1.0f, 0.0f)
    {
    }

    // Initialize the physical parameters.
    //
    // Here f is the natural frequency (speed), z is the damping coefficient (1
    // is critical, <1 is bouncy, >1 is sluggish), and r is the initial
    // response.
    //
    second_order_dynamics (T i, float f, float z, float r)
      : y_ (i),
        v_ (T ()),
        t_ (i)
    {
      set_params (f, z, r);
    }

    void
    set_params (float f, float z, float r)
    {
      MINE_PRECONDITION (f > 0.0f);

      f_ = f;
      z_ = z;
      r_ = r;

      float pi (3.141592653589793f);
      float pf (pi * f_);

      k1_ = z_ / pf;
      k2_ = 1.0f / (4.0f * pi * pi * f_ * f_);
      k3_ = (r_ * z_) / (2.0f * pf);
    }

    void
    set_target (const T& t)
    {
      t_ = t;
    }

    // Instantly warp to a value, killing all momentum in the process.
    //
    void
    reset (const T& v)
    {
      y_ = v;
      v_ = T ();
      t_ = v;
    }

    T
    update (float d, const T& t)
    {
      t_ = t;
      return update (d);
    }

    T
    update (float d)
    {
      MINE_PRECONDITION (d >= 0.0f);

      if (d <= 0.0f)
        return y_;

      // Euler integration is notoriously unstable for stiff springs. So if our
      // time step exceeds the system's safe limit (k2_), we subdivide the step.
      // This guarantees we never explode to infinity.
      //
      float sd (d);

      if (sd > k2_)
      {
        int n (static_cast<int> (std::ceil (sd / k2_)));
        sd = d / static_cast<float> (n);

        for (int i (0); i < n; ++i)
          step (sd);
      }
      else
      {
        step (sd);
      }

      return y_;
    }

    const T&
    value () const
    {
      return y_;
    }

    const T&
    velocity () const
    {
      return v_;
    }

    const T&
    target () const
    {
      return t_;
    }

    bool
    is_settled (float p = 0.001f, float v = 0.001f) const
    {
      T d (t_ - y_);
      return mag (d) < p && mag (v_) < v;
    }

  private:
    void
    step (float d)
    {
      // Estimate the target velocity based on positional change. We clamp the
      // delta to avoid division by zero.
      //
      T td ((t_ - y_) * (1.0f / std::max (d, 0.0001f)));

      T a ((t_ + k3_ * td - y_ - k1_ * v_) * (1.0f / k2_));

      v_ = v_ + a * d;
      y_ = y_ + v_ * d;
    }

    static float
    mag (float v)
    {
      return std::abs (v);
    }

    template <typename U>
    static auto
    mag (const U& v) -> decltype (v.length ())
    {
      return v.length ();
    }

  private:
    T y_; // position
    T v_; // velocity
    T t_; // target

    float f_;
    float z_;
    float r_;

    float k1_;
    float k2_;
    float k3_;
  };

  using float_dynamics = second_order_dynamics<float>;
}
