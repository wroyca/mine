#include <mine/mine-async-loop.hxx>

using namespace std;

namespace mine
{
  async_loop::
  async_loop (const int h)
    : context_ (h),
      work_guard_ (boost::asio::make_work_guard (context_))
  {
    MINE_PRECONDITION (h >= 0);

    MINE_POSTCONDITION (work_guard_.has_value ());
    MINE_POSTCONDITION (!context_.stopped ());

    MINE_INVARIANT (!work_guard_.has_value () ||
                    work_guard_->get_executor () == context_.get_executor ());
  }

  void async_loop::
  run ()
  {
    MINE_PRECONDITION (!context_.stopped ());
    MINE_PRECONDITION (work_guard_.has_value ());

    MINE_INVARIANT (!work_guard_.has_value () ||
                    work_guard_->get_executor () == context_.get_executor ());

    context_.run ();

    MINE_POSTCONDITION (!work_guard_.has_value () || context_.stopped ());
  }

  void async_loop::
  stop () noexcept
  {
    MINE_PRECONDITION (work_guard_.has_value ());

    MINE_INVARIANT (!work_guard_.has_value () ||
                    work_guard_->get_executor () == context_.get_executor ());

    work_guard_.reset ();

    MINE_POSTCONDITION (!work_guard_.has_value ());
  }

  async_loop::context_type& async_loop::
  context () noexcept
  {
    MINE_INVARIANT (!work_guard_.has_value () ||
                    work_guard_->get_executor () == context_.get_executor ());

    return context_;
  }

  async_loop::executor_type async_loop::
  executor () noexcept
  {
    MINE_INVARIANT (!work_guard_.has_value () ||
                    work_guard_->get_executor () == context_.get_executor ());

    return context_.get_executor ();
  }

  async_loop::strand_type async_loop::
  make_strand ()
  {
    MINE_INVARIANT (!work_guard_.has_value () ||
                    work_guard_->get_executor () == context_.get_executor ());

    return strand_type (context_.get_executor ());
  }
}
