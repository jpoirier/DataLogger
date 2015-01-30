// Copyright (c) 2014 Joseph D Poirier
// Distributable under the terms of The New BSD License
// that can be found in the LICENSE file.

#ifndef DEFS_H
#define DEFS_H

#define DO_PRAGMA(x) _Pragma (#x)
#define TODO(x) DO_PRAGMA(message ("TODO - " #x))

#ifdef NDEBUG
 #ifndef _STATIC_ASSERT_
  #define static_assert(e) \
    do { \
      enum {static_assert__ = 1/(e)}; \
    } while (0)
 #endif

 #ifndef  _STATIC_ASSERT_MSG_
  #define static_assert_msg(e, msg) \
    do { \
      enum { static_assert_msg__ ## msg = 1/(e)}; \
    } while (0)
 #endif
#endif

// Standard printf
#ifdef NPRINTF
    #define PRINTF(fmt)
    #define PRINTF_VA(fmt, ...)
#else
# ifdef __TESTING__
    #define PRINTF(fmt)                 pout.putf(fmt)
    #define PRINTF_VA(fmt, ...)         pout.putf(fmt, __VA_ARGS__)
# else
    #define PRINTF(fmt)                 XPLMDebugString(fmt)
    #define PRINTF_VA(fmt, ...)         XPLMDebugString(fmt, __VA_ARGS__)
# endif
#endif

// Debug printf
#ifdef NDEBUG
    #define DPRINTF(fmt)
    #define DPRINTF_VA(fmt, ...)
#else
# ifdef __TESTING__
    #define DPRINTF(fmt)                pout.putf(fmt)
    #define DPRINTF_VA(fmt, ...)        pout.putf(fmt, __VA_ARGS__)
# else
    #define DPRINTF(fmt)                XPLMDebugString(fmt)
    #define DPRINTF_VA(fmt, ...)
# endif
#endif

#ifdef LOGPRINTF
    #define LPRINTF(fmt)                XPLMDebugString(fmt)
    #define LPRINTF_VA(fmt, ...)
#else
    #define LPRINTF(fmt)
    #define LPRINTF_VA(fmt, ...)
#endif

// Error printf
#ifdef NEPRINTF
    #define EPRINTF(fmt)
    #define EPRINTF_VA(fmt, ...)
#else
# ifdef __TESTING__
    #define EPRINTF(fmt)                pout.putf(fmt)
    #define EPRINTF_VA(fmt, ...)        pout.putf(fmt, __VA_ARGS__)
# else
    #define EPRINTF(fmt)                XPLMDebugString(fmt)
    #define EPRINTF_VA(fmt, ...)        XPLMDebugString(fmt, __VA_ARGS__)
# endif
#endif

// CommandHandler pre-event and post-event designators
#define CMD_HNDLR_PROLOG (1)
#define CMD_HNDLR_EPILOG (0)

#define IGNORED_EVENT (0)
#define PROCESSED_EVENT (1)


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif  /* DEFS_H */

