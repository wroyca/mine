#include <mine/mine-vm-bridge.hxx>

#include <type_traits>

#include <mine/mine-editor-core.hxx>

using namespace std;

namespace mine
{
  #define MINE_BIND(mf) integral_constant<decltype(mf), mf>{}

  void
  register_core_api (vm& v)
  {
    vector<native_binding> bindings;

    api_family f (bindings);

    f["bind"] += MINE_BIND (&core::bind_key);
    f["message"] += MINE_BIND (&core::show_message);
    f["quit"] += MINE_BIND (&core::quit);

    v.register_module ("mine", bindings);
  }

  #undef MINE_BIND
}
