#ifndef MVMM_SPINLOCK_H
#define MVMM_SPINLOCK_H

#include "aarch64.h"
#include "types.h"
#include "log.h"

#define SPINLOCK_DEBUG

#ifdef SPINLOCK_DEBUG

struct spinlock {
  int cpuid;
  u8 lock;
  char *name;
};

typedef struct spinlock spinlock_t;

static inline int holdinglk(spinlock_t *lk) {
  if(lk->lock && lk->cpuid == cpuid())
    return 1;
  else
    return 0;
}

static inline void __spinlock_init_dbg(spinlock_t *lk, char *name) {
  lk->cpuid = -1;
  lk->lock = 0;
  lk->name = name;
}

#define spinlock_init(lk) __spinlock_init_dbg(lk, #lk)

#else

typedef u8 spinlock_t;

static inline void __spinlock_init(spinlock_t *lk) {
  *lk = 0;
}

#define spinlock_init(lk) __spinlock_init(lk)

#endif  /* SPINLOCK_DEBUG */

void acquire(spinlock_t *lk);

static inline void release(spinlock_t *lk) {
#ifdef SPINLOCK_DEBUG
  if(!holdinglk(lk))
    panic("release@%s: invalid", lk->name);

  lk->cpuid = -1;
  asm volatile("str wzr, %0" : "=m"(lk->lock) :: "memory");
#else
  asm volatile("str wzr, %0" : "=m"(*lk) :: "memory");
#endif

  isb();
}

#endif
