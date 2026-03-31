#include <mine/mine-async-input.hxx>

using namespace std;

namespace mine
{
  async_input::
  async_input (async_loop& l, event_callback e)
    : stream_ (l.context ()),
      event_callback_ (move (e)),
      buffer_ ()
  {
    MINE_PRECONDITION (event_callback_);

#if defined(_WIN32)
    stream_.assign (::GetStdHandle (STD_INPUT_HANDLE));
#else
    stream_.assign (STDIN_FILENO);
    stream_.non_blocking (true);
#endif

    MINE_POSTCONDITION (stream_.is_open ());
  }

  async_input::
  ~async_input ()
  {
    stop ();
  }

  void async_input::
  start ()
  {
    MINE_PRECONDITION (stream_.is_open ());
    MINE_PRECONDITION (event_callback_);

    if (stream_.is_open ())
    {
      boost::asio::co_spawn (stream_.get_executor (),
                             [this]
      {
        return read ();
      }, boost::asio::detached);
    }
  }

  void async_input::
  stop () noexcept
  {
    if (stream_.is_open ())
    {
      try
      {
        stream_.cancel ();
      }
      catch (const boost::system::system_error& ex)
      {
        cerr << "error cancelling input stream: " << ex.what () << '\n';
      }

      // Release ownership so the destructor doesn't close the fd.
      //
      stream_.release ();
    }

    MINE_POSTCONDITION (!stream_.is_open ());
  }

  boost::asio::awaitable<void> async_input::
  read ()
  {
    MINE_PRECONDITION (stream_.is_open ());
    MINE_PRECONDITION (event_callback_);

    for (;;)
    {
      try
      {
        size_t const n (
          co_await stream_.async_read_some (boost::asio::buffer (buffer_),
                                            boost::asio::use_awaitable));

        MINE_INVARIANT (n > 0);
        MINE_INVARIANT (n <= buffer_.size ());

        parser_.parse (buffer_.data (),
                       n,
                       [this] (input_event e)
        {
          MINE_PRECONDITION (event_callback_);

          // We still need to post the callback to the executor.
          //
          // The parser can emit multiple events in one go (sticky input). If we
          // don't bounce through the executor, we end up running application
          // logic synchronously in a tight loop before getting back to the
          // render cycle.
          //
          // Also note that we mark the lambda mutable because we move 'ev' out
          // of the capture block when invoking the callback, which alters the
          // lambda's internal capture state. If it wasn't mutable, the captured
          // 'ev' would be implicitly const.
          //
          boost::asio::post (stream_.get_executor (),
                             [this, ev (move (e))] () mutable
          {
            MINE_PRECONDITION (event_callback_);

            event_callback_ (move (ev));
          });
        });
      }
      catch (const boost::system::system_error& e)
      {
        // If we were explicitly canceled (e.g., via stop ()), then bail out
        // of the loop.
        //
        if (e.code () == boost::asio::error::operation_aborted)
          break;

        cerr << "error reading input stream: " << e.what () << '\n';
        break;
      }
    }
  }
}
