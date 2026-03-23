#pragma once

#include <cstdint>

namespace mine
{
  // Strongly-typed GPU resource handle.
  //
  // Note that OpenGL uses plain integers for everything. If we accidentally
  // pass a shader ID to a texture binding call, the driver will usually
  // just silently fail or do something unpredictable. We wrap the raw ID
  // in a tag type to catch such mix-ups at compile time instead.
  //
  template <typename T>
  struct gpu_handle
  {
    std::uint32_t id;

    // Zero is the universal invalid ID in OpenGL, so we default to it.
    //
    constexpr gpu_handle ()
      : id (0)
    {
    }

    constexpr explicit gpu_handle (std::uint32_t i)
      : id (i)
    {
    }

    [[nodiscard]] bool
    valid () const
    {
      return id != 0;
    }

    explicit
    operator bool () const
    {
      return valid ();
    }

    bool
    operator == (const gpu_handle&) const = default;

    // Reset back to the invalid state.
    //
    void
    reset ()
    {
      id = 0;
    }
  };

  // Tag types for specific resources.
  //
  struct vertex_buf_tag  {};
  struct index_buf_tag   {};
  struct uniform_buf_tag {};
  struct shader_prog_tag {};
  struct texture_2d_tag  {};
  struct vertex_arr_tag  {};

  // Handle aliases.
  //
  using vertex_buf_handle  = gpu_handle<vertex_buf_tag>;
  using index_buf_handle   = gpu_handle<index_buf_tag>;
  using uniform_buf_handle = gpu_handle<uniform_buf_tag>;
  using shader_prog_handle = gpu_handle<shader_prog_tag>;
  using texture_2d_handle  = gpu_handle<texture_2d_tag>;
  using vertex_arr_handle  = gpu_handle<vertex_arr_tag>;
}
