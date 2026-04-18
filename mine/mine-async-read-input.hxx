#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#if defined(_WIN32)
#  include <boost/asio/windows/stream_handle.hpp>
#else
#  include <boost/asio/posix/stream_descriptor.hpp>
#endif

#include <mine/mine-terminal-input.hxx>

namespace mine
{
  // A channel type for cross-thread input event dispatching.
  //
  // We use this to route events from the dedicated input thread over to the
  // main thread. Since the input coroutine writes from one executor and the
  // main thread reads from another, we need a reliable, thread-safe conduit.
  //
  // It turns out experimental::concurrent_channel fits the bill perfectly here
  // without us having to invent our own synchronized queue.
  //
  using input_channel = boost::asio::experimental::concurrent_channel<
    void (boost::system::error_code, input_event)>;

  // Read raw bytes from stdin on the input executor, parse them into
  // input_event objects, and send them through the channel to the main
  // executor.
  //
  // The idea here is to keep this fully interrupt-driven. We suspend waiting
  // for the OS input, wake up to decode the bytes, and then forward the events
  // into the channel. Notice that there is no polling involved, in that we rely
  // entirely on the executor's reactor to wake us up when there is actual data
  // to process.
  //
  // Note also the platform-specific stream handle types. Windows uses
  // stream_handle while POSIX expects stream_descriptor, so we provide
  // conditionally compiled declarations based on the target OS.
  //
#if defined(_WIN32)
  boost::asio::awaitable<void>
  async_read_input (boost::asio::windows::stream_handle& stream,
                    input_channel& events);
#else
  boost::asio::awaitable<void>
  async_read_input (boost::asio::posix::stream_descriptor& stream,
                    input_channel& events);
#endif
}
