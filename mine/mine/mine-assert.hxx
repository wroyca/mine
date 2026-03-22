#pragma once

#include <iostream>
#include <exception>
#include <source_location>

// #include <cpptrace/cpptrace.hpp>

namespace mine
{
  // Note: we are using C++20 source_location to avoid the macro mess where
  // possible, but we still need macros to stringify the condition expression
  // itself.

  // The panic button.
  //
  // This is the implementation detail for our assertions. We mark it inline
  // to avoid ODR violations in header-only usage, and [[noreturn]] to help
  // the compiler optimize the branches leading here (and silence "control
  // reaches end of non-void function" warnings).
  //
  [[noreturn]] inline void
  fail (const char* expr,
        std::source_location l)
  {
    // Write to cerr. We could use a fancy diagnostic object here, but raw
    // stderr is robust enough for panic situations.
    //
    std::cerr << l.file_name () << ":"
              << l.line () << ":"
              << l.column () << ": "
              << "error: assertion '" << expr << "' failed" << "\n"
              << "  function: " << l.function_name () << "\n";

    // Trace generation is expensive, but since we are terminating anyway,
    // latency doesn't matter.
    //
    std::cerr << "  stack trace:\n";
    // cpptrace::generate_trace ().print ();
    std::cerr << std::endl;

    std::terminate ();
  }

  // The actual check. Note that we rely on the compiler to optimize the
  // branch prediction here (C++20 [[unlikely]]).
  //
  inline void
  check (bool cond,
         const char* expr,
         std::source_location l = std::source_location::current ())
  {
    if (!cond) [[unlikely]]
      fail (expr, l);
  }

  // Semantic wrappers.
  //
  // While these currently map to the same check implementation, we keep them
  // distinct to allow for future differentiation (e.g., stripping preconditions
  // in release builds while keeping invariants).
  //

  inline void
  invariant (bool c,
             const char* e,
             std::source_location l = std::source_location::current ())
  {
    check (c, e, l);
  }

  inline void
  precondition (bool c,
                const char* e,
                std::source_location l = std::source_location::current ())
  {
    check (c, e, l);
  }

  inline void
  postcondition (bool c,
                 const char* e,
                 std::source_location l = std::source_location::current ())
  {
    check (c, e, l);
  }

  // This is for code paths that should theoretically never execute (default
  // cases in exhaustive switches, after infinite loops, etc).
  //
  [[noreturn]] inline void
  unreachable (const char* msg = "unreachable code executed",
               std::source_location l = std::source_location::current ())
  {
    fail (msg, l);
  }
}

// Macros to capture the expression string. We prefer to namespace these
// clearly to avoid collisions with standard assert() or other libs.
//
#define MINE_VERIFY(cond) \
  ::mine::check ((cond), #cond)

#define MINE_INVARIANT(cond) \
  ::mine::invariant ((cond), #cond)

#define MINE_PRECONDITION(cond) \
  ::mine::precondition ((cond), #cond)

#define MINE_POSTCONDITION(cond) \
  ::mine::postcondition ((cond), #cond)

#define MINE_UNREACHABLE() \
  ::mine::unreachable ()
