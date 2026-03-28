#pragma once

#include <cstdlib>
#include <cstdio>

#if !defined(MINE_CONTRACT_MODE_CHECK)  &&                                     \
    !defined(MINE_CONTRACT_MODE_ASSUME) &&                                     \
    !defined(MINE_CONTRACT_MODE_NONE)
#  ifdef MINE_DEVELOP
#    define MINE_CONTRACT_MODE_CHECK
#  else
#    define MINE_CONTRACT_MODE_ASSUME
#  endif
#endif

namespace mine
{
  namespace detail
  {
    [[noreturn]] inline void
    violation (const char* type,
               const char* expr,
               const char* file,
               int         line,
               const char* func) noexcept
    {
      std::fprintf (stderr,
                    "mine: %s violation: %s\n"
                    "  at %s:%d in %s\n",
                    type, expr, file, line, func);
      std::abort ();
    }
  }
}

#if defined(MINE_CONTRACT_MODE_CHECK)

#  define MINE_PRECONDITION(expr)                                              \
     do {                                                                      \
       if (!(expr))                                                            \
         ::mine::detail::violation (                                           \
           "precondition", #expr, __FILE__, __LINE__, __func__);               \
     } while (false)

#  define MINE_POSTCONDITION(expr)                                             \
     do {                                                                      \
       if (!(expr))                                                            \
         ::mine::detail::violation (                                           \
           "postcondition", #expr, __FILE__, __LINE__, __func__);              \
     } while (false)

#  define MINE_INVARIANT(expr)                                                 \
     do {                                                                      \
       if (!(expr))                                                            \
         ::mine::detail::violation (                                           \
           "invariant", #expr, __FILE__, __LINE__, __func__);                  \
     } while (false)

#  define MINE_UNREACHABLE()                                                   \
     do {                                                                      \
       ::mine::detail::violation (                                             \
         "unreachable", "false", __FILE__, __LINE__, __func__);                \
     } while (false)

#elif defined(MINE_CONTRACT_MODE_ASSUME)

#  if defined(__GNUC__) || defined(__clang__)
#    define MINE_ASSUME(expr) \
       do { if (!(expr)) __builtin_unreachable (); } while (false)
#  elif defined(_MSC_VER)
#    define MINE_ASSUME(expr) __assume (expr)
#  else
#    define MINE_ASSUME(expr) ((void)0)
#  endif

#  define MINE_PRECONDITION(expr)  MINE_ASSUME (expr)
#  define MINE_POSTCONDITION(expr) MINE_ASSUME (expr)
#  define MINE_INVARIANT(expr)     MINE_ASSUME (expr)
#  define MINE_UNREACHABLE()       MINE_ASSUME (false)

#else // MINE_CONTRACT_MODE_NONE

#  define MINE_PRECONDITION(expr)  ((void)0)
#  define MINE_POSTCONDITION(expr) ((void)0)
#  define MINE_INVARIANT(expr)     ((void)0)

#  if defined(__GNUC__) || defined(__clang__)
#    define MINE_UNREACHABLE() __builtin_unreachable ()
#  elif defined(_MSC_VER)
#    define MINE_UNREACHABLE() __assume (false)
#  else
#    define MINE_UNREACHABLE() ((void)0)
#  endif

#endif
