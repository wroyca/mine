#include <mine/mine-window-opengl-dynamic-state-timer.hxx>
#include <mine/mine-assert.hxx>

namespace mine
{
  float
  current_time_seconds ()
  {
    time_point n (clock::now ());
    return std::chrono::duration<float> (n.time_since_epoch ()).count ();
  }

  float
  delta_time (time_point s, time_point e)
  {
    return std::chrono::duration<float> (e - s).count ();
  }

  frame_timer::
  frame_timer ()
    : last_ (clock::now ()),
      fc_ (0),
      acc_ (0.0f),
      fps_ (0.0f)
  {
  }

  float frame_timer::
  begin_frame ()
  {
    time_point n (clock::now ());
    float dt (delta_time (last_, n));

    // Protect against pathological clock jumps or debugger pauses.
    // We cap the frame delta to 100ms so our physics solvers don't explode.
    //
    if (dt > 0.1f)
      dt = 0.1f;

    last_ = n;

    fc_++;
    acc_ += dt;

    if (acc_ >= 1.0f)
    {
      fps_ = static_cast<float> (fc_) / acc_;
      fc_ = 0;
      acc_ = 0.0f;
    }

    return dt;
  }
}
