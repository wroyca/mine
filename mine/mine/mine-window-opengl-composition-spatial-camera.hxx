#pragma once

#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>
#include <mine/mine-window-opengl-composition-linear-algebra-mat4.hxx>
#include <mine/mine-window-opengl-dynamic-motion-second-order.hxx>

namespace mine
{
  // A 2D orthographic camera primarily used for viewport rendering.
  //
  // We use it to manage the scroll position and zoom levels, handling the
  // mapping from world coordinates back to screen coordinates and vice versa.
  //
  class camera_2d
  {
  public:
    camera_2d () = default;

    explicit
    camera_2d (vec2 vp);

    void
    set_viewport (vec2 sz);

    vec2
    viewport () const
    {
      return vp_;
    }

    void
    set_position (vec2 p);

    void
    set_position_smooth (vec2 p);

    vec2
    position () const
    {
      return p_.value ();
    }

    void
    set_zoom (float z);

    void
    set_zoom_smooth (float z);

    float
    zoom () const
    {
      return z_.value ();
    }

    void
    scroll_smooth (vec2 d);

    void
    clamp_scroll (float min_y, float max_y);

    void
    make_visible (vec2 wp, float top, float bottom, float left, float right);

    vec2
    screen_to_world (vec2 scr) const;

    vec2
    world_to_screen (vec2 wld) const;

    mat4
    view_projection () const;

    void
    update (float dt);

    bool
    is_animating () const;

  private:
    vec2 vp_ {1024.0f, 768.0f};

    second_order_dynamics<vec2>  p_ {vec2 (0.0f), 4.0f, 1.0f, 0.0f};
    second_order_dynamics<float> z_ {1.0f, 4.0f, 1.0f, 0.0f};
  };
}
