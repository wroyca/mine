#include <mine/mine-async-read-input.hxx>

#include <array>
#include <iostream>
#include <vector>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <mine/mine-contract.hxx>

using namespace std;
namespace asio = boost::asio;

namespace mine
{
  namespace
  {
    constexpr size_t read_buffer_capacity (4096);
  }

#if defined(_WIN32)
  asio::awaitable<void>
  async_read_input (asio::windows::stream_handle& stream, input_channel& events)
#else
  asio::awaitable<void>
  async_read_input (asio::posix::stream_descriptor& stream,
                    input_channel& events)
#endif
  {
    MINE_PRECONDITION (stream.is_open ());

    array<char, read_buffer_capacity> buf;
    terminal_input_parser parser;

    for (;;)
    {
      // Suspend the coroutine and wait for raw bytes from the operating system.
      //
      // Note that this is purely interrupt-driven, so we are not consuming any
      // CPU while blocked waiting for the user to type.
      //
      auto [ec, n] (
        co_await stream.async_read_some (asio::buffer (buf),
                                         asio::as_tuple (asio::use_awaitable)));

      if (ec)
      {
        // It is perfectly normal for the read operation to be aborted if the
        // underlying stream is closed during application shutdown. See if that
        // is the case and bail out.
        //
        if (ec == asio::error::operation_aborted)
          co_return;

        // Otherwise, we have a genuine read error. We log it and attempt to
        // continue, though depending on the error severity, the stream might be
        // fundamentally broken.
        //
        cerr << "async_read_input: " << ec.message () << '\n';
        continue;
      }

      MINE_INVARIANT (n > 0);
      MINE_INVARIANT (n <= buf.size ());

      // Parse the raw terminal bytes into discrete input events. We accumulate
      // them into a batch rather than forwarding them one by one to avoid
      // excessive channel synchronization overhead.
      //
      vector<input_event> batch;

      parser.parse (buf.data (),
                    n,
                    [&batch] (input_event e)
      {
        batch.push_back (move (e));
      });

      // Forward each parsed event through the channel to the main executor.
      //
      for (auto& ev : batch)
      {
        // Note that we pass a default-constructed error_code to async_send so
        // we can capture channel closure gracefully instead of throwing.
        //
        auto [sec] (
          co_await events.async_send (boost::system::error_code (),
                                      move (ev),
                                      asio::as_tuple (asio::use_awaitable)));

        // If the receiver has been closed (for example, if the application is
        // shutting down and dropped the other end), we simply bail out.
        //
        if (sec)
          co_return;
      }
    }
  }
}
