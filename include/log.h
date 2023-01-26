#ifndef LOG_H
#define LOG_H

#include "printf.h"

// #define NDEBUG

#define LWARN     1
#define LLOG      2

#define LOGPREFIX(l)    "\001" #l

#define WARN      LOGPREFIX(1)
#define LOG       LOGPREFIX(2)

#define vmm_warn(...) printf(WARN __VA_ARGS__)

#ifdef NDEBUG

#define vmm_log(...)  (void)0

#else   /* !NDEBUG */

#define vmm_log(...)  printf(LOG __VA_ARGS__)

#endif  /* NDEBUG */

#define vmm_warn_on(cond, ...)  \
  do {    \
    if((cond))    \
      vmm_warn(__VA_ARGS__);   \
  } while(0)

#define vmm_bug_on(cond, ...)   \
  do {    \
    if((cond))    \
      panic(__VA_ARGS__);   \
  } while(0)

#endif  /* LOG_H */
