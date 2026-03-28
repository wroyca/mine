#pragma once

#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif

#if defined(__cpp_deleted_function) && __cpp_deleted_function >= 202403L
#  define MINE_CPP_DELETED_FUNCTION(reason) delete (#reason)
#else
#  define MINE_CPP_DELETED_FUNCTION(reason) delete
#endif
