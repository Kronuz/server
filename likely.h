/*
 * Branch-prediction hints. Self-contained, vendored verbatim from the Kronuz
 * family (compressors/storage). Used as `if unlikely(cond)` / `if likely(cond)`.
 */

#pragma once

#if defined(__has_builtin)
#  if __has_builtin(__builtin_expect)
#    define SERVER_HAVE_BUILTIN_EXPECT 1
#  endif
#elif defined(__GNUC__)
#  define SERVER_HAVE_BUILTIN_EXPECT 1
#endif

#ifdef SERVER_HAVE_BUILTIN_EXPECT
#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
