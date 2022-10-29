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

#define acquire_irqsave(lk, flags)  \
  do {    \
    flags = __acquire_irqsave(lk);    \
  } while(0)

#define release_irqrestore(lk, flags)   \
  do {    \
    __release_irqrestore(lk, flags);    \
  } while(0)

static inline void acquire(spinlock_t *lk) {
  u8 tmp;
  u8 l = 1;

#ifdef SPINLOCK_DEBUG
  if(holdinglk(lk))
    panic("acquire@%s: already held", lk->name);

  asm volatile(
    "sevl\n"
    "1: wfe\n"
    "2: ldaxrb %w0, [%1]\n"
    "cbnz   %w0, 1b\n"
    "stxrb  %w0, %w2, [%1]\n"
    "cbnz   %w0, 2b\n"
    : "=&r"(tmp) : "r"(&lk->lock), "r"(l) : "memory"
  );

  lk->cpuid = cpuid();
#else
  asm volatile(
    "sevl\n"
    "1: wfe\n"
    "2: ldaxrb %w0, [%1]\n"
    "cbnz   %w0, 1b\n"
    "stxrb  %w0, %w2, [%1]\n"
    "cbnz   %w0, 2b\n"
    : "=&r"(tmp) : "r"(lk), "r"(l) : "memory"
  );
#endif

  isb();
}

static inline u64 __acquire_irqsave(spinlock_t *lk) {
  u64 flags = read_sysreg(daif);

  local_irq_disable();

  acquire(lk);

  return flags;
}

static inline void release(spinlock_t *lk) {
#ifdef SPINLOCK_DEBUG
  if(!holdinglk(lk))
    panic("release@%s: invalid", lk->name);

  lk->cpuid = -1;
  asm volatile("stlrb wzr, %0" : "=m"(lk->lock) :: "memory");
#else
  asm volatile("stlrb wzr, [%0]" : "r"(lk) :: "memory");
#endif

  isb();
}

static inline void __release_irqrestore(spinlock_t *lk, u64 flags) {
  release(lk);

  write_sysreg(daif, flags);
}

#endif
