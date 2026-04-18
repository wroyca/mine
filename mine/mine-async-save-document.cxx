#include <mine/mine-async-save-document.hxx>

#include <boost/asio/stream_file.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <mine/mine-contract.hxx>

using namespace std;
namespace asio = boost::asio;

namespace mine
{
  asio::awaitable<void>
  async_save_document (const string& path, const content& text)
  {
    MINE_PRECONDITION (!path.empty ());

    auto executor (co_await asio::this_coro::executor);

    asio::stream_file file (executor,
                            path,
                            asio::stream_file::write_only |
                              asio::stream_file::create   |
                              asio::stream_file::truncate);

    string data (content_to_string (text));

    co_await asio::async_write (file, asio::buffer (data), asio::use_awaitable);
  }
}
