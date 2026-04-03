#include <map>
#include <deque>
#include <numeric>

#include <mine/mine-diagnostics.hxx>

using namespace std;

namespace mine
{
  namespace
  {
    // We need somewhere to stash the samples.
    //
    // Since this diagnostics module is currently designed for single-threaded
    // checks (mainly measuring the responsiveness of the main input loop),
    // a static global map is fine. If we ever start profiling worker threads,
    // we'll need to wrap this in a mutex or move to thread-local storage.
    //
    // Also, use deque for the sliding window; vector::erase(begin) is painful.
    //
    using sample_buffer = deque<double>;
    using storage_map = map<diagnostics::event_type, sample_buffer>;

    static storage_map samples_;
  }

  void
  diagnostics::record_event (event_type t, timestamp start, timestamp end)
  {
    // We work in milliseconds because nanoseconds are too noisy and seconds
    // are too coarse for UI latency.
    //
    double ms ((end.nanoseconds - start.nanoseconds) / 1'000'000.0);

    auto& buf (samples_[t]);
    buf.push_back (ms);

    // Cap the history. 1000 samples is enough to get a stable average without
    // eating too much memory.
    //
    if (buf.size () > 1000)
      buf.pop_front ();
  }

  double
  diagnostics::average_latency_ms (event_type t)
  {
    const auto& buf (samples_[t]);

    if (buf.empty ())
      return 0.0;

    double sum (accumulate (buf.begin (), buf.end (), 0.0));
    return sum / buf.size ();
  }

  void
  diagnostics::reset ()
  {
    samples_.clear ();
  }
}
