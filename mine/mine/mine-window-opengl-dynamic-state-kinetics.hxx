#pragma once

#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>
#include <mine/mine-window-opengl-dynamic-state-timer.hxx>

namespace mine
{
  // Kinetic state for 2D motion.
  //
  // Captures the full derivative stack for an entity. We use this when we
  // need to feed raw forces into an integrator manually.
  //
  struct kinetic_state_2d
  {
    vec2       p  {0.0f, 0.0f}; // Position.
    vec2       v  {0.0f, 0.0f}; // Velocity.
    vec2       a  {0.0f, 0.0f}; // Acceleration.
    time_point ts {clock::now ()};

    void
    touch ()
    {
      ts = clock::now ();
    }

    float
    elapsed () const
    {
      return delta_time (ts, clock::now ());
    }
  };

  // Scroll state.
  //
  // Track viewport momentum so we can flick the scroll wheel and watch the
  // text glide to a halt rather than stopping abruptly.
  //
  struct scroll_state
  {
    vec2 off {0.0f, 0.0f};
    vec2 v   {0.0f, 0.0f};
    vec2 tgt {0.0f, 0.0f};
    bool mom {false}; // Momentum active.

    bool
    is_animating () const
    {
      return mom && v.length () > 0.1f;
    }
  };

  // Cursor blink state.
  //
  // Think of this as a simple square wave oscillator driven by the clock.
  //
  struct blink_state
  {
    time_point last {clock::now ()};
    float      per  {0.5f}; // Period.
    bool       vis  {true}; // Visible.
    bool       en   {true}; // Enabled.

    void
    update ()
    {
      // Bail out early if we are not enabled. Just make sure we stay visible.
      //
      if (!en)
      {
        vis = true;
        return;
      }

      // Calculate how much time elapsed since the last flip.
      //
      float e (delta_time (last, clock::now ()));

      // Flip the visibility state if we have crossed the period threshold.
      //
      if (e >= per)
      {
        vis = !vis;
        last = clock::now ();
      }
    }

    void
    reset ()
    {
      vis = true;
      last = clock::now ();
    }
  };
}
