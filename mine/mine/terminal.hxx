#pragma once

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace mine
{
  class terminal_error : std::system_error
  {
  public:
    terminal_error (int e) : system_error (e, std::generic_category ()) {}

#ifdef _WIN32
    terminal_error (const std::string& d, int fallback_errno_code = 0)
      : system_error (fallback_errno_code, std::system_category (), d) {}
#endif
  };

  template <typename H>
  struct terminal_traits
  {
    using handle_type = H;
  };

  template <typename H, typename T = terminal_traits<H>>
  class basic_terminal
  {
  public:
    using traits_type = T;
    using handle_type = typename traits_type::handle_type;

    explicit
    basic_terminal (handle_type);

  private:
    handle_type handle_{};
  };

  // File descriptor for terminal operations. On POSIX systems, this typically
  // refers to stdin (0), stdout (1), or stderr (2). On Windows, this
  // represents console handles.
  //
#ifdef _WIN32
  using terminal_handle = void*; // HANDLE
#else
  using terminal_handle = int;   // file descriptor
#endif

  using terminal = basic_terminal<terminal_handle>;
}
