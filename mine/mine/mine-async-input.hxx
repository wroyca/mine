#pragma once

#include <array>
#include <functional>
#include <iostream>
#include <utility>

#include <boost/asio.hpp>
#if defined(_WIN32)
#  include <boost/asio/windows/stream_handle.hpp>
#  include <windows.h>
#else
#  include <boost/asio/posix/stream_descriptor.hpp>
#  include <unistd.h>
#endif

#include <boost/system/system_error.hpp>

#include <mine/mine-async-loop.hxx>
#include <mine/mine-terminal-input.hxx>
#include <mine/mine-terminal-input-impl.hxx>
#include <mine/mine-terminal-input.hxx>

#include <mine/mine-predefs.hxx>

namespace mine
{
  // Asynchronous terminal input reader.
  //
  // This thing sits on standard input and feeds raw bytes into the parser.
  //
  // Note that we don't own the underlying file descriptor (stdin/0), so we
  // have to be careful not to close it when we go away.
  //
  class async_input
  {
  public:
    using event_callback = std::move_only_function<void (input_event)>;

    explicit
    async_input (async_loop& l, event_callback e)
      : s_ (l.context ()),
        e_ (std::move (e)),
        b_ ()
    {
#if defined(_WIN32)
      s_.assign (::GetStdHandle (STD_INPUT_HANDLE));
#else
      s_.assign (STDIN_FILENO);
      s_.non_blocking (true);
#endif
    }

    async_input (const async_input&) = MINE_CPP_DELETED_FUNCTION (
      "async_input captures 'this' and cannot be copied");

    async_input (async_input&&) = MINE_CPP_DELETED_FUNCTION (
      "async_input captures 'this' and cannot be moved");

    async_input&
    operator = (const async_input&) = MINE_CPP_DELETED_FUNCTION (
      "async_input captures 'this' and cannot be copied");

    async_input&
    operator = (async_input&&) = MINE_CPP_DELETED_FUNCTION (
      "async_input captures 'this' and cannot be moved");

    ~async_input ()
    {
      stop ();
    }

    void
    start ()
    {
      if (s_.is_open ())
        boost::asio::co_spawn (s_.get_executor (),
                               [this] { return read (); },
                               boost::asio::detached);
    }

    void
    stop () noexcept
    {
      if (s_.is_open ())
      {
        try
        {
          s_.cancel ();
        }
        catch (const boost::system::system_error& ex)
        {
          std::cerr << "error cancelling input stream: " << ex.what () << '\n';
        }

        // Release ownership so the destructor doesn't close the fd.
        //
        s_.release ();
      }
    }

  private:
    boost::asio::awaitable<void>
    read ()
    {
      for (;;)
      {
        try
        {
          std::size_t const n (
            co_await s_.async_read_some (boost::asio::buffer (b_),
                                         boost::asio::use_awaitable));

          if (n > 0)
          {
            p_.parse (b_.data (),
                      n,
                      [this] (input_event e)
            {
              // We still need to post the callback to the executor.
              //
              // The parser can emit multiple events in one go (sticky input).
              // If we don't bounce through the executor, we end up running
              // application logic synchronously in a tight loop before getting
              // back to the render cycle.
              //
              // Also note that we mark the lambda mutable because we move 'ev'
              // out of the capture block when invoking the callback, which
              // alters the lambda's internal capture state. If it wasn't
              // mutable, the captured 'ev' would be implicitly const.
              //
              boost::asio::post (s_.get_executor (),
                                 [this, ev (std::move (e))] () mutable
              {
                e_ (std::move (ev));
              });
            });
          }
        }
        catch (const boost::system::system_error& e)
        {
          // Handle EOF and cancellation.
          //
          // EOF on stdin in raw mode is tricky. It usually means the pty
          // vanished (but how?). For now, let's just keep spinning.
          //
          if (e.code () == boost::asio::error::eof ||
              e.code () == boost::asio::error::broken_pipe)
            continue;

          // If we were explicitly cancelled (e.g., via stop ()), then bail out
          // of the loop.
          //
          if (e.code () == boost::asio::error::operation_aborted)
            break;

          std::cerr << "error reading input stream: " << e.what () << '\n';
          break;
        }
      }
    }

  private:
    static constexpr std::size_t b_n = 4096;

#ifdef _WIN32
    boost::asio::windows::stream_handle s_;
#else
    boost::asio::posix::stream_descriptor s_;
#endif

    terminal_input_parser p_;
    event_callback e_;

    std::array<char, b_n> b_;
  };

  using async_input_handler = async_input;
}
