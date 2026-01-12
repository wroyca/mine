#pragma once

#include <array>
#include <functional>

#include <boost/asio/read.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include <mine/mine-async-loop.hxx>
#include <mine/mine-terminal-input.hxx>
#include <mine/mine-terminal-input-impl.hxx>

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
    using event_callback = std::function<void (input_event)>;

    async_input (async_loop& l, event_callback c)
      : stream_ (l.context ()),
        parser_ (),
        callback_ (std::move (c))
    {
      // Note that we are attaching to STDIN_FILENO since we've already put the
      // terminal into raw mode.
      //
      // Note: assign() can fail if the descriptor is invalid, though for stdin
      // that would be quite catastrophic.
      //
      boost::system::error_code ec;
      ec = stream_.assign (STDIN_FILENO, ec);

      if (!ec)
        ec = stream_.non_blocking (true, ec);
    }

    // We absolutely must detach from the descriptor. If we let the stream_
    // destructor run while it thinks it's open, it will close(0), which
    // is rude to the rest of the process.
    //
    ~async_input ()
    {
      stop ();
    }

    void
    start ()
    {
      if (stream_.is_open ())
        read ();
    }

    // Stop reading and detach from the file descriptor.
    //
    void
    stop ()
    {
      if (stream_.is_open ())
      {
        // Cancel any pending async operations first so the handler runs
        // with operation_aborted.
        //
        boost::system::error_code ec;
        ec = stream_.cancel (ec);

        // Release ownership so the destructor doesn't close the fd.
        //
        stream_.release ();
      }
    }

  private:
    void
    read ()
    {
      stream_.async_read_some (
        boost::asio::buffer (buf_),
        [this] (const boost::system::error_code& ec, std::size_t n)
        {
          // If we were stopped explicitly, we'll get operation_aborted.
          // Don't restart the read loop in that case.
          //
          if (ec == boost::asio::error::operation_aborted)
            return;

          if (!ec && n > 0)
          {
            // We got some data. Parse it and fire events using the
            // callback-based parse to avoid allocating a vector.
            //
            parser_.parse (buf_.data (), n, callback_);

            read ();
          }
          else if (ec == boost::asio::error::eof)
          {
            // EOF on stdin is weird in raw mode. It usually means the
            // pty went away or the user hit ^D in cooked mode. For now,
            // let's try to keep reading, but we might want to back off
            // if this spins.
            //
            read ();
          }
          // else: genuine error, stop reading.
          //
        });
    }

  private:
    boost::asio::posix::stream_descriptor stream_;
    terminal_input_parser parser_;
    event_callback callback_;

    // 256 bytes is plenty for manual input. Even a fast typist or a
    // pasted generic command probably won't overflow this often.
    //
    // @@: or so we'd like to believe, this is an assumption, not a guarantee.
    //
    std::array<char, 256> buf_;
  };

  // Alias for convenience.
  //
  using async_input_handler = async_input;
}
