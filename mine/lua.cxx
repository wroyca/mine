#include <mine/lua.hxx>

#include <iostream>

using namespace std;

namespace mine
{
  lua::
  lua ()
  {
    // Lua handles errors with exceptions when compiling as C++ code.
    //
    try
    {
      state.reset (luaL_newstate ());

      // Opens all standard Lua libraries into the given state by default.
      //
      try
      {
        luaL_openlibs (state);
      }

      catch (const exception& e)
      {
        cerr << "error: unable to open lua standard libraries: "
             << e.what () << endl;

        return;
      }
    }

    catch (const exception& e)
    {
      // Ignore invalid state (but log the error), as the editor can still
      // function in bare mode without Lua support.
      //
      cerr << "error: unable to create lua state: "
           << e.what () << endl;

      return;
    }
  }
}
