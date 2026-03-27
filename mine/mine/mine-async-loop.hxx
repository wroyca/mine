#pragma once

#include <concepts>
#include <optional>

#include <boost/asio.hpp>

#include <mine/mine-assert.hxx>

namespace mine
{
  // The event loop wrapper.
  //
  // Basically, this wraps boost::asio::io_context but adds a "work guard".
  // Without the guard, io_context::run() returns immediately if there is
  // nothing to do. Since this is an interactive application, we want the
  // loop to block and wait for input events (stdin, network, etc.) until
  // we explicitly tell it to stop.
  //
  class async_loop
  {
  public:
    using context_type = boost::asio::io_context;
    using executor_type = context_type::executor_type;
    using strand_type = boost::asio::strand<executor_type>;

    // We default the concurrency hint to 1.
    //
    // If we left this at default, Asio would assume we might call run() from
    // multiple threads and would use internal locking (mutexes) for safety.
    // Since we are running a single-threaded loop, that locking is just wasted
    // CPU cycles and latency.
    //
    explicit
    async_loop (const int h = 1)
      : c_ (h),
        w_ (boost::asio::make_work_guard (c_))
    {
    }

    async_loop (const async_loop&)
      = MINE_DELETE_WITH_REASON("async_loop owns an io_context and cannot be copied");

    async_loop (async_loop&&)
      = MINE_DELETE_WITH_REASON("async_loop owns an io_context and cannot be moved");

    async_loop& operator = (const async_loop&)
      = MINE_DELETE_WITH_REASON("async_loop owns an io_context and cannot be copied");

    async_loop& operator = (async_loop&&)
      = MINE_DELETE_WITH_REASON("async_loop owns an io_context and cannot be moved");

    ~async_loop () = default;

    // Block until stopped and all queued work is finished.
    //
    void
    run ()
    {
      c_.run ();
    }

    // Stop the loop gracefully.
    //
    // We do this by destroying the work guard. This informs the context that no
    // new "external" work is coming. It will process any handlers already in
    // the queue (draining) and then exit run() naturally.
    //
    // Note that we deliberately avoid ctx_.stop() here. That function causes
    // run() to return immediately, potentially discarding pending handlers
    // (like half-finished writes or close events), which is rarely what we
    // want.
    //
    // Note also that if we want to actually exit run(), we must first confirm
    // that no other async operations (like the terminal input reader) are
    // keeping the loop alive.
    //
    void
    stop () noexcept
    {
      w_.reset ();
    }

    // Return the raw context since most Boost.Asio primitives need a reference
    // to it during initialization.
    //
    context_type&
    context () noexcept
    {
      return c_;
    }

    executor_type
    executor () noexcept
    {
      return c_.get_executor ();
    }

    strand_type
    make_strand ()
    {
      return strand_type (c_.get_executor ());
    }

    void
    post (std::invocable auto&& f)
    {
      boost::asio::post (c_, std::forward<decltype (f)> (f));
    }

    void
    dispatch (std::invocable auto&& f)
    {
      boost::asio::dispatch (c_, std::forward<decltype (f)> (f));
    }

  private:
    context_type c_;
    std::optional<boost::asio::executor_work_guard<executor_type>> w_;
  };
}
