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
    using event_callback = std::move_only_function<void (input_event)>;

  public:
    explicit
    async_input (async_loop& l, event_callback e);

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

    ~async_input ();

    void
    start ();

    void
    stop () noexcept;

  private:
    boost::asio::awaitable<void>
    read ();

  private:
    static constexpr std::size_t           buffer_capacity_ {4096};
#ifdef _WIN32
    boost::asio::windows::stream_handle    stream_;
#else
    boost::asio::posix::stream_descriptor  stream_;
#endif
    terminal_input_parser                  parser_;
    event_callback                         event_callback_;
    std::array<char, buffer_capacity_>     buffer_;
  };
}
