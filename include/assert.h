#ifndef CORE_ASSERT_H
#define CORE_ASSERT_H

#include "panic.h"

#define assert(cond)    __assert(__func__, __LINE__, (cond))

#define __assert(fn, ln, cond)                          \
  do {                                                  \
    if(!(cond)) {                                       \
      panic("assert failed in %s:%d: " #cond, fn, ln);  \
    }                                                   \
  } while(0)

#define static_assert(cond) \
  (void)(sizeof(struct { int:-!(cond); }))

#endif
