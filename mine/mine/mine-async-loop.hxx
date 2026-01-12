#pragma once

#include <optional>
#include <functional>

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

    async_loop ()
      : ctx_ (),
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

    // Run the loop. This blocks until stop() is called.
    //
    void
    run ()
    {
      ctx_.run ();
    }

    // Stop the loop.
    //
    // We do this by destroying the work guard to allows the context to
    // drain any remaining handlers and then exit run() naturally.
    //
    void
    stop ()
    {
      work_.reset ();
      ctx_.stop ();
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
