#include <mine/mine-vm-bridge.hxx>

#include <type_traits>

#include <mine/mine-editor.hxx>

using namespace std;

namespace mine
{
  #define MINE_BIND(mf) integral_constant<decltype(mf), mf>{}

  void
  register_core_api (vm& v)
  {
    vector<native_binding> bindings;

    api_family f (bindings);

    f["bind"] += MINE_BIND (&editor::bind_key);
    f["message"] += MINE_BIND (&editor::show_message);
    f["quit"] += MINE_BIND (&editor::quit);

    v.register_module ("mine", bindings);
  }

  #undef MINE_BIND
}
