#include <mine/lua.hxx>

#include <iostream>
#include <print>

using namespace std;

namespace mine
{
  lua::
  lua ()
  {
    try
    {
      state.reset (luaL_newstate ());
    }

    catch (const std::exception& e)
    {
      // It may be acceptable to ignore an invalid state, as the editor can
      // still function in a "bare" mode without Lua support.
      //
      println (cerr, "unable to create lua state: {}", e.what ());
    }
  }
}
