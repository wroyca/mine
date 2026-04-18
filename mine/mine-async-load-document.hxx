#pragma once

#include <string>

#include <boost/asio.hpp>

#include <mine/mine-content.hxx>

namespace mine
{
  // Load a file from disk into an immutable content object.
  //
  // We rely on asio::stream_file here. On Linux, this gets us io_uring support
  // under the hood, meaning the coroutine simply suspends and waits for the
  // kernel to finish the read.
  //
  // Note that on platforms without proper async file I/O, Asio might have to
  // emulate this (e.g., using a thread pool), but from our perspective it
  // remains a completely asynchronous operation.
  //
  // Throw boost::system::system_error on I/O failure (for instance, if the file
  // doesn't exist or we lack the necessary permissions).
  //
  boost::asio::awaitable<content>
  async_load_document (const std::string& path);
}
