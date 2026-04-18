#pragma once

#include <concepts>
#include <cstddef>
#include <string_view>
#include <optional>

#include <mine/mine-types.hxx>

namespace mine
{
  // Forward declarations.
  //
  class content;
  class cursor;
  class viewport;

  // Domain Concepts.
  //

  // A text_content is any type that provides line-based text access.
  //
  template <typename T>
  concept text_content = requires (const T& t, line_number ln)
  {
    { t.line_count () } -> std::convertible_to<std::size_t>;
    { t.line_at (ln) } -> std::same_as<const typename T::line&>;
  };

  // A navigable type can move a cursor in four directions.
  //
  template <typename T>
  concept navigable = requires (const T& t, const content& b)
  {
    { t.move_left (b) } -> std::same_as<T>;
    { t.move_right (b) } -> std::same_as<T>;
    { t.move_up (b) } -> std::same_as<T>;
    { t.move_down (b) } -> std::same_as<T>;
  };

  // A scrollable type can adjust its visible region.
  //
  template <typename T>
  concept scrollable = requires (const T& t, std::size_t n, const content& b,
                                 const cursor& c)
  {
    { t.scroll_to_cursor (c, b) } -> std::same_as<T>;
    { t.scroll_up (n, b) } -> std::same_as<T>;
    { t.scroll_down (n, b) } -> std::same_as<T>;
  };
}
