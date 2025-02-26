#pragma once

#include <memory>

// Prevent C++ from name-mangling Lua symbols so the linker can find them
// correctly.
//
extern "C"
{
  #include <lualib.h>
  #include <lauxlib.h>
}

namespace mine
{
  namespace detail
  {
    struct lua_state_deleter
    {
      void
      operator() (lua_State* p) const noexcept
      {
        // Destroys all objects in the given Lua state (calling the
        // corresponding garbage-collection meta methods, if any) and frees
        // all dynamic memory used by this state.
        //
        lua_close (p);
      }
    };
  }

  class lua
  {
  public:
    explicit
    lua ();

  private:
    // Lua keeps all its state in this dynamic structure and a pointer to this
    // structure is passed as an argument to all its functions.
    //
    std::unique_ptr <lua_State, detail::lua_state_deleter> state;
  };
}
