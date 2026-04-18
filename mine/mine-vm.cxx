#include <mine/mine-vm.hxx>

#include <cstring>
#include <iostream>

#include <lua.hpp>

#include <mine/mine-contract.hxx>

using namespace std;

namespace mine
{
  namespace
  {
    // Native C bridge that replaces Lua's default print.
    //
    int
    lua_print_bridge (lua_State* l)
    {
      int n (lua_gettop (l));
      string r;

      // We need Lua's tostring function to format the arguments.
      //
      lua_getglobal (l, "tostring");

      for (int i (1); i <= n; ++i)
      {
        lua_pushvalue (l, -1); // push tostring.
        lua_pushvalue (l, i);  // push argument.
        lua_call (l, 1, 1);

        size_t z;
        const char* p (lua_tolstring (l, -1, &z));

        // If tostring doesn't return a string, we have to bail out. This
        // matches the standard Lua print behavior.
        //
        if (p == nullptr)
          return luaL_error (l, "'tostring' must return a string to 'print'");

        // Print normally spaces out multiple arguments with a tab character.
        //
        if (i > 1)
          r += '\t';

        r.append (p, z);

        lua_pop (l, 1); // pop the string result.
      }

      lua_pop (l, 1); // pop tostring.

      // Look up our injected handler pointer. We store it as light user data.
      //
      // Note that if it is missing for some reason, we fall back to stdout
      // rather than failing silently.
      //
      lua_getglobal (l, "__mine_print_handler");

      if (lua_islightuserdata (l, -1))
      {
        using cb = function<void (string_view)>;
        auto* h (static_cast<cb*> (lua_touserdata (l, -1)));
        (*h) (r);
      }
      else
        cout << r << '\n';

      lua_pop (l, 1);
      return 0;
    }
  }

  // script_result
  //

  script_result script_result::
  ok_with_value (string v)
  {
    return script_result {true, move (v), nullopt};
  }

  script_result script_result::
  ok_void ()
  {
    return script_result {true, nullopt, nullopt};
  }

  script_result script_result::
  fail (string e)
  {
    return script_result {false, nullopt, move (e)};
  }

  // vm_limits
  //

  vm_limits vm_limits::
  default_limits ()
  {
    return vm_limits {};
  }

  // For untrusted scripts, we lock everything down.
  //
  vm_limits vm_limits::
  restrictive ()
  {
    return vm_limits {
      100000,            // 100k instructions.
      4 * 1024 * 1024,   // 4 MB.
      100,               // Shallow call depth.
      false,             // No IO.
      false,             // No OS.
      false,             // No debug.
      false              // No loadstring.
    };
  }

  // For trusted scripts (e.g., internal configs), we can be generous with
  // resources and capabilities.
  //
  vm_limits vm_limits::
  permissive ()
  {
    return vm_limits {
      100000000,          // 100M instructions.
      256 * 1024 * 1024,  // 256 MB.
      1000,               // Deep call depth.
      true,               // Allow IO.
      true,               // Allow OS.
      false,              // Still no debug (usually not needed in prod).
      true                // Allow loadstring so Fennel can evaluate compiled Lua code.
    };
  }

  // vm
  //

  vm::
  vm (vm_limits l)
    : L_ (nullptr),
      limits_ (l),
      state_ (vm_state::uninitialized),
      fennel_loaded_ (false)
  {
  }

  vm::
  ~vm ()
  {
    if (L_ != nullptr)
      shutdown ();
  }

  // Lifecycle management.
  //

  void vm::
  initialize ()
  {
    MINE_PRECONDITION (state_ == vm_state::uninitialized ||
                       state_ == vm_state::shutdown);

    // Try to bring up a fresh Lua state.
    //
    if ((L_ = luaL_newstate ()) == nullptr)
    {
      state_ = vm_state::error;
      last_error_ = "failed to create Lua state";
      return;
    }

    // Load the standard libraries. This gives us strings, tables, math, etc. We
    // will prune the dangerous ones in setup_sandbox() shortly.
    //
    luaL_openlibs (L_);

    setup_sandbox ();
    install_limits ();

    state_ = vm_state::ready;
    fennel_loaded_ = false;
    last_error_ = nullopt;
  }

  void vm::
  shutdown ()
  {
    if (L_ != nullptr)
    {
      lua_close (L_);
      L_ = nullptr;
    }

    state_ = vm_state::shutdown;
    fennel_loaded_ = false;
  }

  void vm::
  reset ()
  {
    shutdown ();
    initialize ();
  }

  // State inquiry.
  //

  vm_state vm::
  state () const
  {
    return state_;
  }

  bool vm::
  is_ready () const
  {
    return state_ == vm_state::ready;
  }

  bool vm::
  is_executing () const
  {
    return state_ == vm_state::executing;
  }

  const vm_limits& vm::
  limits () const
  {
    return limits_;
  }

  // Security and Resource Control.
  //

  // If the vm is configured to be restrictive, we need to manually remove the
  // standard library tables that allow access to the host system (IO, OS,
  // Debug).
  //
  void vm::
  setup_sandbox ()
  {
    MINE_PRECONDITION (L_ != nullptr);

    if (!limits_.allow_io)
    {
      // Nuke the IO library and file loading functions.
      //
      lua_pushnil (L_);
      lua_setglobal (L_, "io");

      lua_pushnil (L_);
      lua_setglobal (L_, "dofile");

      lua_pushnil (L_);
      lua_setglobal (L_, "loadfile");
    }

    if (!limits_.allow_os)
    {
      // We want to keep safe OS functions like clock/date/time, but remove
      // anything that touches the shell or filesystem.
      //
      lua_getglobal (L_, "os");

      if (lua_istable (L_, -1))
      {
        const char* forbidden[] = {
          "execute", "exit", "getenv", "remove",
          "rename", "tmpname", "setlocale", nullptr
        };

        for (const char** p = forbidden; *p != nullptr; ++p)
        {
          lua_pushnil (L_);
          lua_setfield (L_, -2, *p);
        }
      }
      lua_pop (L_, 1); // os table
    }

    if (!limits_.allow_debug)
    {
      lua_pushnil (L_);
      lua_setglobal (L_, "debug");
    }

    if (!limits_.allow_loadstring)
    {
      // 'load' can load bytecode which is a vector for exploits.
      //
      lua_pushnil (L_);
      lua_setglobal (L_, "load");

      lua_pushnil (L_);
      lua_setglobal (L_, "loadstring");
    }
  }

  // Placeholder for resource limits.
  //
  void vm::
  install_limits ()
  {
    MINE_PRECONDITION (L_ != nullptr);
  }

  // Fennel Integration.
  //

  // We need to load that compiler into our state before we can run any .fnl
  // code.
  //
  script_result vm::
  load_fennel (string_view src)
  {
    MINE_PRECONDITION (is_ready ());
    MINE_PRECONDITION (L_ != nullptr);

    state_ = vm_state::executing;

    // Load fennel.lua chunk onto the stack.
    //
    if (luaL_loadbuffer (L_, src.data (), src.size (), "fennel.lua") != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "unknown load error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    // Execute the chunk. It returns the fennel module table.
    //
    if (lua_pcall (L_, 0, 1, 0) != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "unknown execution error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    // Verify we actually got the module back.
    //
    if (!lua_istable (L_, -1))
    {
      lua_pop (L_, 1);
      state_ = vm_state::ready;
      return script_result::fail ("fennel.lua did not return a module table");
    }

    // Store it in the global registry as 'fennel'.
    //
    lua_setglobal (L_, "fennel");

    state_ = vm_state::ready;
    fennel_loaded_ = true;
    last_error_ = nullopt;

    return script_result::ok_void ();
  }

  bool vm::
  fennel_loaded () const
  {
    return fennel_loaded_;
  }

void vm::
  add_fennel_path (string_view p)
  {
    MINE_PRECONDITION (fennel_loaded_);
    MINE_PRECONDITION (is_ready ());

    // Grab the fennel module table.
    //
    lua_getglobal (L_, "fennel");

    if (lua_istable (L_, -1))
    {
      lua_getfield (L_, -1, "path");

      // The path really should be a string, but let's be defensive here just in
      // case someone messed with the fennel table underneath us.
      //
      string cp (lua_isstring (L_, -1) ? lua_tostring (L_, -1) : "");
      lua_pop (L_, 1);

      string np (string (p) + ";" + cp);
      lua_pushstring (L_, np.c_str ());
      lua_setfield (L_, -2, "path");
    }

    lua_pop (L_, 1);
  }

  void vm::
  set_print_handler (function<void (string_view)>* h)
  {
    MINE_PRECONDITION (is_ready ());

    // Tuck the raw pointer to the function away in a global so that our static
    // bridge can actually retrieve it when Lua tries to print something.
    //
    lua_pushlightuserdata (L_, h);
    lua_setglobal (L_, "__mine_print_handler");

    // Finally, clobber the base environment's print function with our own
    // bridge.
    //
    lua_pushcfunction (L_, lua_print_bridge);
    lua_setglobal (L_, "print");
  }

  // Execution.
  //

  // The core execution logic. We wrap loadbuffer/pcall and try to normalize the
  // result into our script_result type.
  //
  script_result vm::
  execute_chunk (string_view code, const char* name)
  {
    MINE_PRECONDITION (L_ != nullptr);
    MINE_PRECONDITION (state_ == vm_state::ready);

    state_ = vm_state::executing;

    // Load.
    //
    if (luaL_loadbuffer (L_, code.data (), code.size (), name) != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "unknown load error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    // Execute.
    //
    if (lua_pcall (L_, 0, LUA_MULTRET, 0) != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "unknown execution error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    // Capture the return value if one exists. We only support basic scalars
    // for now.
    //
    optional<string> val;

    if (lua_gettop (L_) > 0)
    {
      int type (lua_type (L_, -1));

      if (type == LUA_TSTRING)
        val = lua_tostring (L_, -1);
      else if (type == LUA_TNUMBER)
        val = to_string (lua_tonumber (L_, -1));
      else if (type == LUA_TBOOLEAN)
        val = lua_toboolean (L_, -1) ? "true" : "false";
      else if (type == LUA_TNIL)
        val = "nil";

      // Clear the stack so we are clean for the next run.
      //
      lua_settop (L_, 0);
    }

    state_ = vm_state::ready;
    last_error_ = nullopt;

    return val
      ? script_result::ok_with_value (*val)
      : script_result::ok_void ();
  }

  script_result vm::
  execute_lua (string_view code)
  {
    return execute_chunk (code, "lua_code");
  }

  // To execute Fennel, we delegate to the `fennel.eval` function inside the Lua
  // state.
  //
  script_result vm::
  execute_fennel (string_view code)
  {
    MINE_PRECONDITION (fennel_loaded_);
    MINE_PRECONDITION (is_ready ());

    // Stack: [ ... ]
    lua_getglobal (L_, "fennel"); // Stack: [ ..., fennel_tab ]

    if (!lua_istable (L_, -1))
    {
      lua_pop (L_, 1);
      return script_result::fail ("fennel module not loaded");
    }

    lua_getfield (L_, -1, "eval"); // Stack: [ ..., fennel_tab, eval_func ]

    if (!lua_isfunction (L_, -1))
    {
      lua_pop (L_, 2);
      return script_result::fail ("fennel.eval not found");
    }

    // We have the function, remove the table to clean up the stack.
    // Stack: [ ..., eval_func ]
    lua_remove (L_, -2);

    lua_pushlstring (L_, code.data (), code.size ()); // Stack: [ ..., eval_func, code_str ]

    state_ = vm_state::executing;

    // Call fennel.eval(code).
    //
    if (lua_pcall (L_, 1, 1, 0) != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "fennel evaluation error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    // Parse return value (top of stack).
    //
    optional<string> val;

    if (lua_gettop (L_) > 0)
    {
      if (lua_isstring (L_, -1))
        val = lua_tostring (L_, -1);
      else if (lua_isnumber (L_, -1))
        val = to_string (lua_tonumber (L_, -1));
      else if (lua_isboolean (L_, -1))
        val = lua_toboolean (L_, -1) ? "true" : "false";

      lua_pop (L_, 1);
    }

    state_ = vm_state::ready;
    last_error_ = nullopt;

    return val
      ? script_result::ok_with_value (*val)
      : script_result::ok_void ();
  }

  script_result vm::
  execute_lua_file (string_view path)
  {
    MINE_PRECONDITION (is_ready ());
    MINE_PRECONDITION (limits_.allow_io);

    state_ = vm_state::executing;

    // luaL_dofile is a macro wrapper around loadfile/pcall.
    //
    string s (path);
    if (luaL_dofile (L_, s.c_str ()) != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "file execution error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    state_ = vm_state::ready;
    return script_result::ok_void ();
  }

  script_result
  vm::execute_fennel_file (string_view p)
  {
    MINE_PRECONDITION (fennel_loaded_);
    MINE_PRECONDITION (is_ready ());
    MINE_PRECONDITION (limits_.allow_io);

    // Grab the fennel module from the global state. It should be there, but
    // let's be safe.
    //
    lua_getglobal (L_, "fennel");
    if (!lua_istable (L_, -1))
    {
      lua_pop (L_, 1);
      return script_result::fail ("fennel module not loaded");
    }

    // Extract the dofile function.
    //
    lua_getfield (L_, -1, "dofile");
    if (!lua_isfunction (L_, -1))
    {
      lua_pop (L_, 2);
      return script_result::fail ("fennel.dofile not found");
    }

    // We only need the function itself, so drop the table from the stack.
    //
    lua_remove (L_, -2);

    lua_pushlstring (L_, p.data (), p.size ());

    state_ = vm_state::executing;

    if (lua_pcall (L_, 1, 1, 0) != LUA_OK)
    {
      // Bail out and grab the error. Notice that lua_tostring can technically
      // return null, so we handle that just in case.
      //
      const char* e (lua_tostring (L_, -1));
      last_error_ = e ? string (e) : "fennel execution error";
      lua_pop (L_, 1);

      state_ = vm_state::ready;
      return script_result::fail (*last_error_);
    }

    // Try to salvage a return value if one was left on the stack. We only care
    // about a few basic types here.
    //
    optional<string> r;

    if (lua_gettop (L_) > 0)
    {
      if (lua_isstring (L_, -1))
        r = lua_tostring (L_, -1);
      else if (lua_isnumber (L_, -1))
        r = to_string (lua_tonumber (L_, -1));
      else if (lua_isboolean (L_, -1))
        r = lua_toboolean (L_, -1) ? string ("true") : string ("false");

      lua_pop (L_, 1);
    }

    state_ = vm_state::ready;
    last_error_ = nullopt;

    return r ? script_result::ok_with_value (*r) : script_result::ok_void ();
  }

  script_result vm::
  compile_fennel (string_view code)
  {
    MINE_PRECONDITION (fennel_loaded_);
    MINE_PRECONDITION (is_ready ());

    // Similar to eval, we look up fennel.compileString.
    //
    lua_getglobal (L_, "fennel");
    if (!lua_istable (L_, -1))
    {
      lua_pop (L_, 1);
      return script_result::fail ("fennel module not loaded");
    }

    lua_getfield (L_, -1, "compileString");
    if (!lua_isfunction (L_, -1))
    {
      lua_pop (L_, 2);
      return script_result::fail ("fennel.compileString not found");
    }

    lua_remove (L_, -2); // Remove fennel table.
    lua_pushlstring (L_, code.data (), code.size ());

    if (lua_pcall (L_, 1, 1, 0) != LUA_OK)
    {
      const char* err (lua_tostring (L_, -1));
      last_error_ = err ? string (err) : "fennel compilation error";
      lua_pop (L_, 1);
      return script_result::fail (*last_error_);
    }

    if (!lua_isstring (L_, -1))
    {
      lua_pop (L_, 1);
      return script_result::fail ("fennel.compileString did not return string");
    }

    string res (lua_tostring (L_, -1));
    lua_pop (L_, 1);

    return script_result::ok_with_value (move (res));
  }

  // Binding helpers.
  //

  void vm::
  register_module (string_view name,
                   const vector<native_binding>& bindings)
  {
    MINE_PRECONDITION (L_ != nullptr);
    MINE_PRECONDITION (is_ready ());

    lua_newtable (L_); // The module table.

    for (const auto& b : bindings)
    {
      lua_pushcfunction (L_, b.func);
      lua_setfield (L_, -2, b.name.c_str ());
    }

    string s (name);
    lua_setglobal (L_, s.c_str ());
  }

  void vm::
  set_global_string (string_view name, string_view val)
  {
    MINE_PRECONDITION (L_ != nullptr);

    lua_pushlstring (L_, val.data (), val.size ());
    string s (name);
    lua_setglobal (L_, s.c_str ());
  }

  void vm::
  set_global_number (string_view name, double val)
  {
    MINE_PRECONDITION (L_ != nullptr);

    lua_pushnumber (L_, val);
    string s (name);
    lua_setglobal (L_, s.c_str ());
  }

  void vm::
  set_global_bool (string_view name, bool val)
  {
    MINE_PRECONDITION (L_ != nullptr);

    lua_pushboolean (L_, val ? 1 : 0);
    string s (name);
    lua_setglobal (L_, s.c_str ());
  }

  void vm::
  set_global_userdata (string_view name, void* ptr)
  {
    MINE_PRECONDITION (L_ != nullptr);

    lua_pushlightuserdata (L_, ptr);
    string s (name);
    lua_setglobal (L_, s.c_str ());
  }

  void* vm::
  get_global_userdata (string_view name) const
  {
    MINE_PRECONDITION (L_ != nullptr);

    string s (name);
    lua_getglobal (L_, s.c_str ());

    void* res (nullptr);
    if (lua_islightuserdata (L_, -1))
      res = lua_touserdata (L_, -1);

    lua_pop (L_, 1);
    return res;
  }

  // Diagnostics.
  //

  optional<string> vm::
  last_error () const
  {
    return last_error_;
  }

  void vm::
  clear_error ()
  {
    last_error_ = nullopt;
    if (state_ == vm_state::error)
      state_ = vm_state::ready;
  }

  count_t vm::
  memory_used () const
  {
    if (L_ == nullptr)
      return 0;

    // lua_gc returns KBs and bytes separately.
    //
    return (static_cast<count_t> (lua_gc (L_, LUA_GCCOUNT, 0)) * 1024) +
           (static_cast<count_t> (lua_gc (L_, LUA_GCCOUNTB, 0)));
  }

  void vm::
  collect_garbage ()
  {
    if (L_ != nullptr)
      lua_gc (L_, LUA_GCCOLLECT, 0);
  }

  lua_State* vm::
  raw_state ()
  {
    return L_;
  }

  const lua_State* vm::
  raw_state () const
  {
    return L_;
  }
}
