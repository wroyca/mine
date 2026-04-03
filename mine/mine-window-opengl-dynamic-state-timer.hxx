#pragma once

#include <chrono>
#include <cstddef>

namespace mine
{
  // We use the steady clock. It turns out that relying on the system clock for
  // measuring frame intervals is a recipe for disaster if the host NTP daemon
  // decides to adjust the time mid-execution.
  //
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using duration = std::chrono::duration<float>;

  // Get the current time as float seconds since epoch.
  //
  // We feed this primarily into the render graph shaders or absolute timeline
  // animations. Note that by using float here, precision will inevitably
  // degrade if the process stays alive for several days. For our typical
  // use cases, though, this is perfectly acceptable and avoids double
  // precision conversion overhead on the GPU.
  //
  float
  current_time_seconds ();

  // Calculate delta time in seconds between two time points.
  //
  float
  delta_time (time_point s, time_point e);

  // A frame timer for consistent pacing.
  //
  // Tracks the delta time between frames and aggregates an FPS counter
  // over one-second intervals.
  //
  class frame_timer
  {
  public:
    frame_timer ();

    // Mark the beginning of a new frame.
    //
    // Returns the delta time (dt) in seconds since the last call.
    //
    float
    begin_frame ();

    // Current frames per second (updated every 1s).
    //
    float
    fps () const { return fps_; }

    // Average frame time in seconds.
    //
    // We guard against division by zero just in case this gets queried
    // before the first full second has accumulated.
    //
    float
    frame_time () const
    {
      return fps_ > 0.0f ? 1.0f / fps_ : 0.0f;
    }

  private:
    time_point  last_;
    std::size_t fc_;  // Frame count.
    float       acc_; // Accumulated time.
    float       fps_;
  };
}
