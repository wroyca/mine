#pragma once

#include <chrono>

#include <mine/mine-types.hxx>

namespace mine
{
  // The stopwatch.
  //
  // Esentially a static facility to track how long things take. We primarily
  // care about the "input-to-photon" loop (keystroke -> render), so we
  // categorize events rather than logging arbitrary strings.
  //
  class diagnostics
  {
  public:
    enum class event_type
    {
      keystroke,
      render,
      command_execution,
      state_transition
    };

    // Record a completed event.
    //
    static void
    record_event (event_type t, timestamp start, timestamp end);

    static double
    average_latency_ms (event_type t);

    static void
    reset ();

    // Clock source.
    //
    // We use steady_clock because we don't care about wall time (NTP jumps),
    // only intervals.
    //
    static timestamp
    now () noexcept
    {
      using namespace std::chrono;

      auto ns (duration_cast<nanoseconds> (
                 steady_clock::now ().time_since_epoch ()).count ());

      return timestamp (static_cast<std::uint64_t> (ns));
    }
  };

  // RAII helper.
  //
  class scoped_timer
  {
  public:
    explicit
    scoped_timer (diagnostics::event_type t)
      : t_ (t),
        start_ (diagnostics::now ())
    {
    }

    ~scoped_timer ()
    {
      diagnostics::record_event (t_, start_, diagnostics::now ());
    }

  private:
    diagnostics::event_type t_;
    timestamp start_;
  };
}
