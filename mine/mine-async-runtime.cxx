#include <mine/mine-async-runtime.hxx>

#include <mine/mine-contract.hxx>

using namespace std;

namespace mine
{
  async_runtime::async_runtime ()
    : main_guard_ (boost::asio::make_work_guard (main_ctx_)),
      input_guard_ (boost::asio::make_work_guard (input_ctx_)),
      input_thread_ ([this] (stop_token)
      {
        input_ctx_.run ();
      })
  {
    MINE_POSTCONDITION (main_guard_.has_value ());
    MINE_POSTCONDITION (input_guard_.has_value ());
    MINE_POSTCONDITION (input_thread_.joinable ());
  }

  async_runtime::
  ~async_runtime ()
  {
    shutdown ();
  }

  void async_runtime::
  run ()
  {
    MINE_PRECONDITION (main_guard_.has_value ());

    main_ctx_.run ();
  }

  void async_runtime::
  shutdown () noexcept
  {
    // First, destroy the work guards. This signals to both contexts that no new
    // work will be arriving and they should drain their respective queues.
    //
    main_guard_.reset ();
    input_guard_.reset ();

    // Force-stop the contexts to unblock any pending async_wait or
    // async_read_some operations. If we don't do this, the threads might hang
    // indefinitely waiting for I/O that will never arrive.
    //
    input_ctx_.stop ();
    main_ctx_.stop ();
  }

  async_runtime::context_type& async_runtime::
  main_context () noexcept
  {
    return main_ctx_;
  }

  async_runtime::context_type& async_runtime::
  input_context () noexcept
  {
    return input_ctx_;
  }

  async_runtime::executor_type async_runtime::
  main_executor () noexcept
  {
    return main_ctx_.get_executor ();
  }

  async_runtime::executor_type async_runtime::
  input_executor () noexcept
  {
    return input_ctx_.get_executor ();
  }
}
