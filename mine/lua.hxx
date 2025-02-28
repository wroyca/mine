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
    template <class T, typename D = std::default_delete <T>> struct
    lua_state : std::unique_ptr <T, D>
    {
      using std::unique_ptr <T, D>::unique_ptr;

      constexpr
      operator lua_State* () const noexcept
      {
        return this->get ();
      }
    };

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
    detail::lua_state <lua_State, detail::lua_state_deleter> state;
  };
}
