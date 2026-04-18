#pragma once

#include <optional>
#include <thread>

#include <boost/asio.hpp>

namespace mine
{
  // Unified async runtime featuring two distinct execution environments.
  //
  // The editor requires two completely independent execution contexts to
  // function properly without stepping on its own toes:
  //
  //   1. The main executor. This handles the core editor logic, rendering, and
  //      file I/O coroutines. The calling thread will block in run() to drive
  //      this context.
  //
  //   2. The input executor. This is bound to a dedicated thread that waits on
  //      OS-level input events. It does absolutely nothing but read stdin and
  //      push events into a channel. The most important rule is that it must
  //      never be delayed by the editor's main workload.
  //
  // Note that we initialize both contexts with a concurrency hint of 1. Since
  // each is strictly driven by a single thread, this hints Boost.Asio to elide
  // internal mutex locking.
  //
  class async_runtime
  {
  public:
    using context_type  = boost::asio::io_context;
    using executor_type = context_type::executor_type;

    async_runtime ();

    // Delete copy and move operations.
    //
    // Managing an OS thread and heavy I/O contexts means this object is
    // fundamentally pinned to its memory location.
    //
    async_runtime (const async_runtime&) = delete;
    async_runtime (async_runtime&&) = delete;

    async_runtime& operator= (const async_runtime&) = delete;
    async_runtime& operator= (async_runtime&&) = delete;

    ~async_runtime ();

    // Block the calling thread on the main context.
    //
    // This serves as our primary event loop driver and will only return once a
    // shutdown is requested and the work queues are fully drained.
    //
    void
    run ();

    // Trigger a graceful shutdown.
    //
    // The sequence here is important. First, we destroy the work guards so that
    // both contexts are allowed to exit run() naturally once their pending
    // queues empty out. Then, we stop the contexts explicitly to unblock any
    // hanging, unbounded waits before the input jthread joins.
    //
    void
    shutdown () noexcept;

    context_type&
    main_context () noexcept;

    context_type&
    input_context () noexcept;

    executor_type
    main_executor () noexcept;

    executor_type
    input_executor () noexcept;

  private:
    context_type main_ctx_ {1};
    context_type input_ctx_ {1};

    // Work guards prevent the io_contexts from exiting their run() loops
    // immediately if they happen to momentarily run out of scheduled work.
    //
    std::optional<boost::asio::executor_work_guard<executor_type>> main_guard_;
    std::optional<boost::asio::executor_work_guard<executor_type>> input_guard_;

    std::jthread input_thread_;
  };
}
