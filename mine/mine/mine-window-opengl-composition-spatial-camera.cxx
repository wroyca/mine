#include <mine/mine-window-opengl-composition-spatial-camera.hxx>
#include <mine/mine-assert.hxx>

#include <algorithm>

namespace mine
{
  camera_2d::
  camera_2d (vec2 v)
    : vp_ (v)
  {
  }

  void camera_2d::
  set_viewport (vec2 s)
  {
    vp_ = s;
  }

  void camera_2d::
  set_position (vec2 p)
  {
    p_.reset (p);
  }

  void camera_2d::
  set_position_smooth (vec2 p)
  {
    p_.set_target (p);
  }

  void camera_2d::
  set_zoom (float z)
  {
    // Clamp the zoom to a sensible range to avoid singularity or flipping the
    // projection matrix.
    //
    z_.reset (std::clamp (z, 0.1f, 10.0f));
  }

  void camera_2d::
  set_zoom_smooth (float z)
  {
    z_.set_target (std::clamp (z, 0.1f, 10.0f));
  }

  void camera_2d::
  scroll_smooth (vec2 d)
  {
    p_.set_target (p_.target () + d);
  }

  void camera_2d::
  clamp_scroll (float mn, float mx)
  {
    vec2 t (p_.target ());

    // Restrict the vertical scrolling to the provided boundaries while making
    // sure that we never pan past the left edge.
    //
    t.y = std::clamp (t.y, mn, mx);
    t.x = std::max (t.x, 0.0f);

    p_.set_target (t);
  }

  void camera_2d::
  make_visible (vec2 w, float top, float bottom, float left, float right)
  {
    // The idea here is to calculate the effective viewport size in world
    // coordinates so we know exactly how much we see at the current zoom.
    //
    vec2 s (vp_ / z_.value ());
    vec2 t (p_.target ());

    // Nudge the target position if the point falls outside our designated
    // margin area. We process both axes independently.
    //
    if (w.x < t.x + left)
      t.x = w.x - left;
    else if (w.x > t.x + s.x - right)
      t.x = w.x - s.x + right;
    if (w.y < t.y + top)
      t.y = w.y - top;
    else if (w.y > t.y + s.y - bottom)
      t.y = w.y - s.y + bottom;

    // Finally, prevent the camera from wandering into negative space.
    //
    t.x = std::max (t.x, 0.0f);
    t.y = std::max (t.y, 0.0f);

    p_.set_target (t);
  }

  vec2 camera_2d::
  screen_to_world (vec2 s) const
  {
    return p_.value () + (s / z_.value ());
  }

  vec2 camera_2d::
  world_to_screen (vec2 w) const
  {
    return (w - p_.value ()) * z_.value ();
  }

  mat4 camera_2d::
  view_projection () const
  {
    // Map to standard OpenGL NDC, treating the top-left as the origin.
    //
    mat4 p (mat4::ortho (0.0f, vp_.x, vp_.y, 0.0f, -1.0f, 1.0f));

    // Notice that we  leave out sub-pixel snapping (e.g., via std::round) here.
    // While snapping might prevent texture filtering artifacts, it completely
    // ruins the perception of smooth motion at lower speeds and introduces
    // visible stuttering. Instead, we rely on standard bilinear filtering to do
    // the heavy lifting.
    //
    mat4 v (mat4::translate (-p_.value ().x, -p_.value ().y, 0.0f) *
            mat4::scale (z_.value (), z_.value (), 1.0f));

    return p * v;
  }

  void camera_2d::
  update (float dt)
  {
    p_.update (dt);
    z_.update (dt);
  }

  bool camera_2d::
  is_animating () const
  {
    return !p_.is_settled () || !z_.is_settled ();
  }
}
