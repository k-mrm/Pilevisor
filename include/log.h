#ifndef LOG_H
#define LOG_H

#include "printf.h"

// #define vmm_log(...)  (void)0

#define WARN      "\001" "1"
#define VSMLOG    "\001" "2"
#define LOG       "\001" "3"

#define vmm_log(...)  printf(LOG __VA_ARGS__)
#define vsm_log(...)  printf(VSMLOG __VA_ARGS__)
#define vmm_warn(...) printf(WARN __VA_ARGS__)

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

#endif  /* LOG_H */
