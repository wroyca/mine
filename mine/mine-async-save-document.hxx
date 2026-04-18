#pragma once

#include <mine/mine-content.hxx>

#include <boost/asio.hpp>

#include <string>

namespace mine
{
  // Save the content object to disk.
  //
  // Note that we use asio::stream_file here which is backed by io_uring
  // on Linux for true kernel-level asynchronous I/O. The idea is that the
  // coroutine simply suspends while the kernel does the actual writes, so
  // we don't end up blocking any threads or polling behind our backs.
  //
  // Throw boost::system::system_error if the underlying I/O fails.
  //
  boost::asio::awaitable<void>
  async_save_document (const std::string& path, const content& text);
}
