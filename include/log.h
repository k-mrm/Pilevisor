#ifndef MVMM_LOG_H
#define MVMM_LOG_H

#include "printf.h"

#define vmm_log(...)  (void)0

// #define vmm_log(...)  printf("[vmm-log] " __VA_ARGS__)
#define vmm_warn(...) printf("[vmm-warn] " __VA_ARGS__)

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

#define build_bug_on(cond) \
  (void)(sizeof(struct { int:-!!(cond); }))


#endif
