#include <mine/mine-async-load-document.hxx>

#include <array>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <mine/mine-contract.hxx>

using namespace std;
namespace asio = boost::asio;

namespace mine
{
  asio::awaitable<content>
  async_load_document (const string& path)
  {
    MINE_PRECONDITION (!path.empty ());

    // Obtain the executor from the current coroutine context. We will need it
    // to initialize our asynchronous stream file.
    //
    auto executor (co_await asio::this_coro::executor);

    asio::stream_file file (executor, path, asio::stream_file::read_only);

    // Set up a buffer to read the file in chunks. 64K is generally a reasonable
    // block size for asynchronous file I/O operations.
    //
    constexpr size_t chunk_size (65536);
    array<char, chunk_size> buf;
    string data;

    for (;;)
    {
      // Suspend the coroutine and read the next chunk. We wrap our completion
      // token in as_tuple to prevent Asio from throwing an exception when we
      // hit EOF. For a file read loop, reaching EOF is the expected termination
      // condition, not an exceptional one.
      //
      auto [ec, n] = co_await file.async_read_some (
        asio::buffer (buf),
        asio::as_tuple (asio::use_awaitable));

      // It is entirely possible for Asio to report both a successful short read
      // and an error (such as EOF) simultaneously in the same completion. So,
      // we must process whatever data we managed to extract first.
      //
      if (n > 0)
        data.append (buf.data (), n);

      if (ec)
      {
        if (ec == asio::error::eof)
          break;

        // If it is not EOF, then something actually went wrong. Bail out.
        //
        throw boost::system::system_error (ec);
      }
    }

    co_return make_content_from_string (data);
  }
}
