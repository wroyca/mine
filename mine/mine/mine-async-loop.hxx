#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

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
    using strand = boost::asio::strand<executor_type>;

    // We default the concurrency hint to 1.
    //
    // If we left this at default, Asio would assume we might call run() from
    // multiple threads and would use internal locking (mutexes) for safety.
    // Since we are running a single-threaded loop, that locking is just wasted
    // CPU cycles and latency.
    //
    explicit
    async_loop (int concurrency_hint = 1)
      : ctx_ (concurrency_hint),
        // Lock the context so run() doesn't exit when the queue is empty.
        //
        work_ (boost::asio::make_work_guard (ctx_))
    {
    }

    // We own the context, so no copying or moving.
    //
    async_loop (const async_loop&) = delete;
    async_loop (async_loop&&) = delete;
    async_loop& operator= (const async_loop&) = delete;
    async_loop& operator= (async_loop&&) = delete;

    // Run the loop. This blocks until stop() is called and all work is
    // finished.
    //
    void
    run ()
    {
      ctx_.run ();
    }

    // Stop the loop gracefully.
    //
    // We do this by destroying the work guard. This informs the context that no
    // new "external" work is coming. It will process any handlers already in
    // the queue (draining) and then exit run() naturally.
    //
    // Note: We deliberately avoid ctx_.stop() here. That function causes run()
    // to return immediately, potentially discarding pending handlers (like
    // half-finished writes or close events), which is rarely what we want.
    //
    // IMPORTANT: To actually exit run(), we must also confirm no other async
    // operations (like the terminal input reader) are keeping the loop alive.
    //
    void
    stop ()
    {
      work_.reset ();
    }

    // Accessors.
    //
    // We expose the raw context because almost all Boost.Asio primitives
    // (sockets, timers) need a reference to it during construction.
    //
    context_type&
    context () { return ctx_; }

    executor_type
    executor () { return ctx_.get_executor (); }

    // Utilities.
    //

    strand
    make_strand ()
    {
      return strand (ctx_.get_executor ());
    }

    template <typename F>
    void
    post (F&& f)
    {
      boost::asio::post (ctx_, std::forward<F> (f));
    }

    template <typename F>
    void
    dispatch (F&& f)
    {
      boost::asio::dispatch (ctx_, std::forward<F> (f));
    }

  private:
    context_type ctx_;
    std::optional<boost::asio::executor_work_guard<executor_type>> work_;
  };

  // Coroutine conveniences.
  //
  // We pull these into our namespace so we don't have to type boost::asio::
  // every time we write a coroutine.
  //
  template <typename T>
  using awaitable = boost::asio::awaitable<T>;

  using boost::asio::use_awaitable;
  using boost::asio::co_spawn;
  using boost::asio::detached;
}
