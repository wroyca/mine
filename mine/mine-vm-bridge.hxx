#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <lua.hpp>

#include <mine/mine-vm.hxx>

namespace mine
{
  class editor;

  // Register native C++ editor APIs into the provided VM.
  //
  // Note that the VM must already have the `__mine_core` global userdata
  // set so that our generated C-thunks can resolve the editor
  // context when invoked.
  //
  void
  register_core_api (vm& v);

  // Forward declaration of the argument caster.
  //
  template <typename T>
  struct lua_arg;

  template <>
  struct lua_arg<std::string_view>
  {
    static std::string_view
    cast (lua_State* l, int idx)
    {
      size_t len (0);
      const char* s (lua_tolstring (l, idx, &len));
      return s != nullptr ? std::string_view (s, len) : std::string_view ();
    }
  };

  template <>
  struct lua_arg<std::string>
  {
    static std::string
    cast (lua_State* l, int idx)
    {
      size_t len (0);
      const char* s (lua_tolstring (l, idx, &len));
      return s != nullptr ? std::string (s, len) : std::string ();
    }
  };

  template <>
  struct lua_arg<bool>
  {
    static bool
    cast (lua_State* l, int idx)
    {
      return lua_toboolean (l, idx) != 0;
    }
  };

  // Template Metaprogramming Machinery.
  //
  // The overall idea here is to automatically generate Lua C API compliant
  // thunks for our C++ member functions. Since Lua is a C library, it expects a
  // function pointer of type `int (*)(lua_State*)`. We cannot pass a C++ member
  // function pointer directly, nor can we use something like `std::function` or
  // lambdas with captures, because they are stateful and cannot decay to a
  // plain C function pointer.
  //
  // To solve this without writing repetitive boilerplate for every bound
  // function, we use C++17 non-type template parameters (`auto Impl`). That is,
  // by passing the member function pointer as a template argument, the compiler
  // instantiates a unique, static C++ function (the `thunk`) for each member
  // function we register. Because the function pointer is part of the type
  // system, the generated thunk is stateless and implicitly decays to the
  // required `lua_CFunction` signature.
  //
  // Note that inside this generated thunk, we must read arguments from the Lua
  // stack and convert them to the expected C++ types. We can achieve this by
  // pattern-matching the member function signature to deduce its argument types
  // (`typename... A`). Once we have the type pack `A...`, we create a
  // compile-time sequence of indices representing the position of each argument
  // using `std::index_sequence_for<A...>`.
  //
  // Now for the actual invocation, it happens in the `dispatch` helper. Here,
  // we rely on C++11 parameter pack expansion. Note that the Lua stack is
  // 1-indexed. Therefore, as we expand the 0-based `std::index_sequence`, we
  // map each index `I` to `I + 1` for our Lua stack access, which then expands
  // into a comma-separated list of arguments. For example, if the C++ function
  // expects `(bool, std::string)`, this expands to: `lua_arg<bool>::cast(l, 1),
  // lua_arg<std::string>::cast(l, 2)`. These extracted values are then passed
  // directly into the member function invocation via the pointer-to-member
  // operator `(c->*Impl)(...)`.
  //
  // Keep in mind that the `lua_arg` template must be specialized for every type
  // we wish to bridge from Lua. If we attempt to bind a member function that
  // takes a currently unsupported type (like an `int` or a custom struct), the
  // compiler will generate an error about a missing `cast` method in the
  // unspecialized base `lua_arg` template.
  //

  template <auto Impl>
  struct function_cast;

  template <typename... A, void (editor::*Impl) (A...)>
  struct function_cast<Impl>
  {
    static int
    thunk (lua_State* l)
    {
      // Recover the editor editor instance.
      //
      lua_getglobal (l, "__mine_core");
      auto* c (static_cast<editor*> (lua_touserdata (l, -1)));
      lua_pop (l, 1);

      if (c == nullptr)
        return 0; // TODO: Should probably luaL_error here.

      return dispatch (l, c, std::index_sequence_for<A...> ());
    }

  private:
    template <std::size_t... I>
    static int
    dispatch (lua_State* l, editor* c, std::index_sequence<I...>)
    {
      // Note that Lua stacks are 1-indexed, so we offset our index sequence
      // by 1 when fetching arguments. We rely on the comma operator and
      // parameter pack expansion to evaluate the arguments in order.
      //
      (c->*Impl) (lua_arg<typename std::decay<A>::type>::cast (l, I + 1)...);

      // Since the return type here is void, we push nothing back to Lua.
      //
      return 0;
    }
  };

  // API Family builder.
  //
  // Syntactic sugar to build our function registry.
  //
  class api_family
  {
  public:
    explicit
    api_family (std::vector<native_binding>& bindings)
      : bindings_ (bindings) {}

    struct entry
    {
      std::vector<native_binding>& bindings;
      std::string name;

      template <auto Impl>
      void
      operator += (std::integral_constant<decltype (Impl), Impl>) const
      {
        bindings.push_back ({name, &function_cast<Impl>::thunk});
      }
    };

    entry
    operator[] (std::string name) const
    {
      return entry {bindings_, std::move (name)};
    }

  private:
    std::vector<native_binding>& bindings_;
  };
}
