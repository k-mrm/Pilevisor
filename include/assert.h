#ifndef CORE_ASSERT_H
#define CORE_ASSERT_H

#include "panic.h"

#define assert(cond)    __assert(__func__, (cond))

#define __assert(fn, cond)                        \
  do {                                            \
    if(!(cond)) {                                 \
      panic("assert failed in %s: " #cond, fn);   \
    }                                             \
  } while(0)

#endif
