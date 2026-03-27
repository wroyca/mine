#pragma once

#include <mine/mine-types.hxx>
#include <mine/mine-assert.hxx>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward declare Lua state (no Lua headers in public API).
//
struct lua_State;

namespace mine
{
  using count_t = std::size_t;    // Count of items.

  // Forward declarations.
  //
  class loop;

  // Script execution result.
  //
  struct script_result
  {
    bool success;
    std::optional<std::string> value;
    std::optional<std::string> error;

    [[nodiscard]] bool
    ok () const
    {
      return success;
    }

    explicit
    operator bool () const
    {
      return ok ();
    }

    static script_result
    ok_with_value (std::string val);

    static script_result
    ok_void ();

    static script_result
    fail (std::string err);
  };

  // Script execution limits (concrete trait object).
  //
  struct vm_limits
  {
    count_t max_instructions    = 1000000;           // Instruction count limit.
    count_t max_memory_bytes    = 16 * 1024 * 1024;  // 16 MB default.
    count_t max_call_depth      = 200;               // Lua call stack depth.
    bool    allow_io            = false;             // File I/O access.
    bool    allow_os            = false;             // OS functions.
    bool    allow_debug         = false;             // Debug library.
    bool    allow_loadstring    = false;             // Dynamic code loading.

    static vm_limits
    default_limits ();

    static vm_limits
    restrictive ();

    static vm_limits
    permissive ();
  };

  // VM state.
  //
  enum class vm_state
  {
    uninitialized,  // Not yet started.
    ready,          // Ready to execute.
    executing,      // Currently running a script.
    error,          // In error state (needs reset).
    shutdown        // Permanently stopped.
  };

  // Native function binding (for bridge to register).
  //
  using native_function = int (*)(lua_State*);

  struct native_binding
  {
    std::string name;
    native_function func;
  };

  // The VM: Lua VM owner and executor.
  //
  class vm
  {
  public:
    // Construct with limits.
    //
    explicit
    vm (vm_limits limits = vm_limits::default_limits ());

    vm (const vm&)
      = MINE_DELETE_WITH_REASON("vm manages unique execution state and hardware resources that cannot be duplicated");

    vm& operator= (const vm&)
      = MINE_DELETE_WITH_REASON("vm manages unique execution state and hardware resources that cannot be duplicated");

    vm (vm&&)
      = MINE_DELETE_WITH_REASON("vm contains internal self-references and stable memory mappings that prevent moving");

    vm& operator= (vm&&)
      = MINE_DELETE_WITH_REASON("vm contains internal self-references and stable memory mappings that prevent moving");

    ~vm ();

    // Lifecycle.
    //
    void
    initialize ();

    void
    shutdown ();

    void
    reset ();

    // State queries.
    //

    [[nodiscard]] vm_state
    state () const;

    [[nodiscard]] bool
    is_ready () const;

    [[nodiscard]] bool
    is_executing () const;

    [[nodiscard]] const vm_limits&
    limits () const;

    // Fennel support.
    //
    // Load the Fennel compiler into the VM. Must be called after initialize()
    // and before executing Fennel code.
    //
    script_result
    load_fennel (std::string_view fennel_source);

    [[nodiscard]] bool
    fennel_loaded () const;

    // Add a directory format to `fennel.path` so (require ...) works nicely for
    // user configuration submodules. Example argument: "~/.config/mine/?.fnl"
    //
    void
    add_fennel_path (std::string_view search_path);

    // Override Lua's default `print` function to route output to a custom
    // handler.
    //
    void
    set_print_handler (std::function<void (std::string_view)>* handler);

    // Script execution.
    //
    // Execute Lua code directly.
    //
    script_result
    execute_lua (std::string_view code);

    // Load and execute a Lua file.
    //
    script_result
    execute_lua_file (std::string_view path);

    // Execute Fennel code (requires fennel_loaded()).
    //
    script_result
    execute_fennel (std::string_view code);

    // Load and execute a Fennel file directly.
    //
    script_result
    execute_fennel_file (std::string_view path);

    // Compile Fennel to Lua (for AOT compilation).
    //
    script_result
    compile_fennel (std::string_view fennel_code);

    // Native bindings.
    //
    // Register a table of native functions accessible from scripts.
    // Must be called after initialize().
    //
    void
    register_module (std::string_view name,
                     const std::vector<native_binding>& bindings);

    // Set a global value (for passing context to scripts).
    //
    void
    set_global_string (std::string_view name, std::string_view value);

    void
    set_global_number (std::string_view name, double value);

    void
    set_global_bool (std::string_view name, bool value);

    // Userdata support (for bridge to attach opaque context).
    //
    void
    set_global_userdata (std::string_view name, void* ptr);

    [[nodiscard]] void*
    get_global_userdata (std::string_view name) const;

    // Error handling.
    //
    [[nodiscard]] std::optional<std::string>
    last_error () const;

    void
    clear_error ();

    // Memory usage.
    //
    [[nodiscard]] count_t
    memory_used () const;

    void
    collect_garbage ();

    // Raw Lua state access.
    //
    [[nodiscard]] lua_State*
    raw_state ();

    [[nodiscard]] const lua_State*
    raw_state () const;

  private:
    lua_State* L_;
    vm_limits limits_;
    vm_state state_;
    bool fennel_loaded_;
    std::optional<std::string> last_error_;

    void
    setup_sandbox ();

    void
    install_limits ();

    script_result
    execute_chunk (std::string_view code, const char* chunk_name);
  };
}
