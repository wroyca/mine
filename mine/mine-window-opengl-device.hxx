#pragma once

#include <mine/mine-window-opengl-common-enums.hxx>
#include <mine/mine-window-opengl-common-handle.hxx>
#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <span>

namespace mine
{
  // Describe a single vertex attribute.
  //
  // Keep in mind that 'sz' represents the number of components (1, 2, 3, or 4),
  // not the size in bytes.
  //
  struct vertex_attr
  {
    std::uint32_t loc;
    std::uint32_t sz;
    std::uint32_t st;
    std::uint32_t off;
    bool          norm {false};
  };

  // Bundle a shader source string with its pipeline stage.
  //
  struct shader_src
  {
    shader_stage     stg;
    std::string_view code;
  };

  // Texture creation descriptor.
  //
  // Provide reasonable defaults here so we don't have to spell everything out
  // for a standard rgba8 linear texture.
  //
  struct tex_desc
  {
    std::uint32_t  w   {0};
    std::uint32_t  h   {0};
    texture_format fmt {texture_format::rgba8};
    texture_filter min {texture_filter::linear};
    texture_filter mag {texture_filter::linear};
    texture_wrap   ws  {texture_wrap::clamp_to_edge};
    texture_wrap   wt  {texture_wrap::clamp_to_edge};
    bool           mip {false};
  };

  // The rendering device.
  //
  // We wrap the global OpenGL state machine into this cohesive, stateless
  // interface. It translates our high-level enums into actual GL calls and
  // takes care of handle generation.
  //
  // Notice that since OpenGL contexts are heavily thread-local, you should
  // only use instances of this class on the thread that originally created
  // the window context. Don't try to pass this around across threads, it
  // won't end well.
  //
  class render_device
  {
  public:
    render_device () = default;

    // Load glad bindings. Must be called immediately after context creation.
    //
    bool
    init ();

    // Buffer ops.
    //

    vertex_buf_handle
    create_vbo (std::size_t sz,
                const void* d,
                buffer_usage u = buffer_usage::static_draw);

    void
    update_vbo (vertex_buf_handle h,
                std::size_t off,
                std::size_t sz,
                const void* d);

    void
    destroy_vbo (vertex_buf_handle h);

    index_buf_handle
    create_ibo (std::size_t sz,
                const void* d,
                buffer_usage u = buffer_usage::static_draw);

    void
    update_ibo (index_buf_handle h,
                std::size_t off,
                std::size_t sz,
                const void* d);

    void
    destroy_ibo (index_buf_handle h);

    // VAO ops.
    //

    vertex_arr_handle
    create_vao ();

    void
    configure_vao (vertex_arr_handle vao,
                   vertex_buf_handle vbo,
                   std::span<const vertex_attr> attrs);

    void
    configure_vao_indexed (vertex_arr_handle vao,
                           vertex_buf_handle vbo,
                           index_buf_handle ibo,
                           std::span<const vertex_attr> attrs);

    void
    destroy_vao (vertex_arr_handle h);

    // Shader ops.
    //

    shader_prog_handle
    create_shader (std::span<const shader_src> srcs);

    shader_prog_handle
    create_shader (std::string_view vs, std::string_view fs);

    void
    destroy_shader (shader_prog_handle h);

    // Uniforms
    //

    void
    set_uniform (shader_prog_handle h, std::string_view n, float v);

    void
    set_uniform (shader_prog_handle h, std::string_view n, const vec2& v);

    void
    set_uniform (shader_prog_handle h, std::string_view n, const vec3& v);

    void
    set_uniform (shader_prog_handle h, std::string_view n, const vec4& v);

    void
    set_uniform (shader_prog_handle h, std::string_view n, int v);

    void
    set_uniform_mat4 (shader_prog_handle h,
                      std::string_view n,
                      const float* d,
                      bool trans = false);

    // Texture ops.
    //

    texture_2d_handle
    create_tex (const tex_desc& desc, const void* d = nullptr);

    void
    update_tex (texture_2d_handle h,
                std::uint32_t x,
                std::uint32_t y,
                std::uint32_t w,
                std::uint32_t ht,
                const void* d,
                texture_format f = texture_format::rgba8);

    void
    destroy_tex (texture_2d_handle h);

    // State binding.
    //

    void
    bind_shader (shader_prog_handle h);

    void
    bind_vao (vertex_arr_handle h);

    void
    bind_tex (texture_2d_handle h, std::uint32_t u = 0);

    // Drawing.
    //

    void
    draw_arrays (primitive_topology t, std::uint32_t f, std::uint32_t c);

    void
    draw_elements (primitive_topology t,
                   std::uint32_t c,
                   std::uint32_t off = 0);

    // Raster state.
    //

    void
    set_viewport (std::int32_t x,
                  std::int32_t y,
                  std::uint32_t w,
                  std::uint32_t h);

    void
    set_scissor (std::int32_t x,
                 std::int32_t y,
                 std::uint32_t w,
                 std::uint32_t h);

    void
    set_scissor_enabled (bool en);

    void
    set_blend_enabled (bool en);

    void
    set_blend_func (blend_factor sr,
                    blend_factor dr,
                    blend_factor sa,
                    blend_factor da);

    void
    clear (float r, float g, float b, float a);

  private:
    std::optional<int>
    get_loc (shader_prog_handle h, std::string_view n);

  private:
    bool init_ {false};
  };
}
