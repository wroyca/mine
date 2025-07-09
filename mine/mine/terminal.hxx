#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <system_error>

namespace mine
{
  struct terminal_error : std::system_error
  {
    terminal_error (int e)
      : system_error (e, std::generic_category ()) {}

#ifdef _WIN32
    terminal_error (const std::string& d, int fallback_errno_code = 0)
      : system_error (fallback_errno_code, std::system_category (), d) {}
#endif
  };

  enum class terminal_input_mode
  {
    canonical,
    raw,
    cbreak
  };

  struct terminal_characteristics
  {
    terminal_input_mode input_mode = terminal_input_mode::canonical;

    terminal_characteristics () = default;
  };

  template <typename H>
  struct terminal_traits
  {
    using handle_type = H;

    static terminal_characteristics
    get_characteristics (const handle_type&);

    static void
    set_characteristics (const handle_type&, const terminal_characteristics&);
  };

  template <typename H, typename T = terminal_traits<H>>
  class basic_terminal
  {
  public:
    using traits_type = T;
    using handle_type = typename traits_type::handle_type;

    explicit
    basic_terminal (handle_type);

    // Terminal state management.
    //
    terminal_characteristics
    characteristics () const;

    void
    characteristics (const terminal_characteristics&);

    // Input mode operations.
    //
    void
    set_input_mode (terminal_input_mode);

    terminal_input_mode
    input_mode () const;

    // Raw mode convenience functions.
    //
    void
    set_raw_mode ();

  private:
    handle_type handle_{};
    std::optional<terminal_characteristics> original_characteristics_;

    void
    save_original_characteristics ();

    void
    restore_original_characteristics ();
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
