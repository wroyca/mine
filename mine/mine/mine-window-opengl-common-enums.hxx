#pragma once

#include <cstdint>

namespace mine
{
  // Render pipeline enumerations.
  //
  // Abstract away the raw GLenum constants so we don't bleed OpenGL headers
  // into the interface. Note that we use std::uint8_t to keep our pipeline
  // state objects as tight as possible.
  //

  // Blend factors.
  //
  // We map one_minus_* to inv_* for brevity.
  //
  enum class blend_factor : std::uint8_t
  {
    zero,
    one,
    src_color,
    one_minus_src_color,
    dst_color,
    one_minus_dst_color,
    src_alpha,
    one_minus_src_alpha,
    dst_alpha,
    one_minus_dst_alpha,
    constant_color,
    one_minus_constant_color,
    constant_alpha,
    one_minus_constant_alpha,
    src_alpha_saturate
  };

  // Blend operations.
  //
  enum class blend_op : std::uint8_t
  {
    add,
    subtract,
    reverse_subtract,
    min,
    max
  };

  // Depth and stencil compare functions.
  //
  enum class compare_func : std::uint8_t
  {
    never,
    less,
    equal,
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always
  };

  // Primitive topology.
  //
  enum class primitive_topology : std::uint8_t
  {
    points,
    lines,
    line_strip,
    triangles,
    triangle_strip,
    triangle_fan
  };

  // Texture formats.
  //
  // Currently we only expose a common subset that maps well across different
  // backends (GL, Vulkan, etc). We might need to extend this later once we
  // introduce compressed formats.
  //
  enum class texture_format : std::uint8_t
  {
    r8,
    rg8,
    rgb8,
    rgba8,
    r16f,
    rg16f,
    rgb16f,
    rgba16f,
    r32f,
    rg32f,
    rgb32f,
    rgba32f,
    depth16,
    depth24,
    depth32f,
    depth24_stencil8
  };

  // Texture filtering modes.
  //
  enum class texture_filter : std::uint8_t
  {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear
  };

  // Texture wrapping behavior.
  //
  enum class texture_wrap : std::uint8_t
  {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
    clamp_to_border
  };

  // Programmable shader stages.
  //
  enum class shader_stage : std::uint8_t
  {
    vertex,
    fragment,
    geometry,
    compute
  };

  // Buffer usage hints.
  //
  enum class buffer_usage : std::uint8_t
  {
    static_draw,
    dynamic_draw,
    stream_draw
  };

  // Face culling mode.
  //
  enum class cull_mode : std::uint8_t
  {
    none,
    front,
    back,
    front_and_back
  };

  // Polygon winding order.
  //
  enum class front_face : std::uint8_t
  {
    clockwise,
    counter_clockwise
  };
}
