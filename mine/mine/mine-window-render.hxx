#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <mine/mine-core-state.hxx>
#include <mine/mine-window-opengl-composition-spatial-camera.hxx>
#include <mine/mine-window-opengl-device.hxx>
#include <mine/mine-window-opengl-typography-atlas.hxx>
#include <mine/mine-window-opengl-typography-raster.hxx>
#include <mine/mine-window-opengl-typography-shape.hxx>
#include <mine/mine-syntax.hxx>

namespace mine
{
  struct text_vertex
  {
    vec2 p;
    vec2 uv;
    vec4 c;
  };

  struct ui_vertex
  {
    vec2  p;
    vec2  uv;
    vec4  c;
    vec2  dim;
    float radius;
  };

  class window_renderer
  {
  public:
    window_renderer ();
    ~window_renderer ();

    window_renderer (const window_renderer&) = delete;
    window_renderer& operator = (const window_renderer&) = delete;

    window_renderer (window_renderer&&) noexcept;
    window_renderer& operator = (window_renderer&&) noexcept;

    bool
    load_font (std::string_view p, std::uint32_t s);

    void
    render (const editor_state& s, bool track = false);

    void
    resize (int w, int h);

    void
    update (float dt);

    void
    scroll (float dx, float dy);

    bool
    is_animating () const;

    // Map a physical pixel coordinate to a terminal-compatible grid coordinate.
    //
    screen_position
    screen_to_grid (float px, float py, const editor_state& s);

    float
    line_height () const
    {
      return rast_.line_height ();
    }

    void
    set_text_color (const vec4& c)
    {
      c_txt_ = c;
    }

    void
    set_cursor_color (const vec4& c)
    {
      c_cur_ = c;
    }

    void
    set_selection_color (const vec4& c)
    {
      c_sel_ = c;
    }

    void
    set_bg_color (const vec4& c)
    {
      c_bg_ = c;
    }

  private:
    void
    ensure_glyph (std::uint32_t cp);

    void
    push_quad (std::vector<text_vertex>& v,
               const vec2& p0,
               const vec2& p1,
               const vec2& uv0,
               const vec2& uv1,
               const vec4& c);

    void
    push_ui_quad (std::vector<ui_vertex>& v,
                  const vec2& p0,
                  const vec2& p1,
                  const vec4& c,
                  float radius);

  private:
    render_device dev_;
    camera_2d cam_;

    ft_rasterizer rast_;
    bin_packer pack_;
    texture_updater up_;

    syntax_highlighter highlighter_;

    shader_prog_handle sh_txt_;
    shader_prog_handle sh_sld_;
    shader_prog_handle sh_ui_;

    vertex_arr_handle vao_txt_;
    vertex_buf_handle vbo_txt_;

    vertex_arr_handle vao_sld_;
    vertex_buf_handle vbo_sld_;

    vertex_arr_handle vao_ui_;
    vertex_buf_handle vbo_ui_;

    vertex_arr_handle vao_ui_txt_;
    vertex_buf_handle vbo_ui_txt_;

    texture_2d_handle tex_atl_;

    std::unordered_map<std::uint32_t, glyph_info> gl_;

    std::vector<text_vertex> v_txt_;
    std::vector<text_vertex> v_sld_;

    std::vector<ui_vertex>   v_ui_;
    std::vector<text_vertex> v_ui_txt_;

    vec4 c_txt_;
    vec4 c_cur_;
    vec4 c_sel_;
    vec4 c_bg_;

    float max_sy_ {1e10f};
  };
}
