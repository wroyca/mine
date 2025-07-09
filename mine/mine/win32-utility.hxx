#pragma once

#ifdef _WIN32
// Try to include <windows.h> so that it doesn't mess other things up.
//
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    ifndef NOMINMAX // No min and max macros.
#      define NOMINMAX
#      include <windows.h>
#      undef NOMINMAX
#    else
#      include <windows.h>
#    endif
#    undef WIN32_LEAN_AND_MEAN
#  else
#    ifndef NOMINMAX
#      define NOMINMAX
#      include <windows.h>
#      undef NOMINMAX
#    else
#      include <windows.h>
#    endif
#  endif
#endif
