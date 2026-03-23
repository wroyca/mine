#include <mine/mine-window-opengl-dynamic-motion-second-order.hxx>

namespace mine
{
  template class second_order_dynamics<float>;
  template class second_order_dynamics<vec2>;
  template class second_order_dynamics<vec3>;
  template class second_order_dynamics<vec4>;
}
