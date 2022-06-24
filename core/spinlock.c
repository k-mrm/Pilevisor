#include "spinlock.h"

void acquire(spinlock_t *lk) {
#ifdef SPINLOCK_DEBUG
  if(holdinglk(lk))
    panic("acquire@%s: already held", lk->name);

  asm volatile(
    "mov x1, %0\n"
    "mov w2, #1\n"
    "1: ldaxr w3, [x1]\n"
    "cbnz w3, 1b\n"
    "stxr w3, w2, [x1]\n"
    "cbnz w3, 1b\n"
    :: "r"(&lk->lock) : "memory"
  );

  lk->cpuid = cpuid();
#else
  asm volatile(
    "mov x1, %0\n"
    "mov w2, #1\n"
    "1: ldaxr w3, [x1]\n"
    "cbnz w3, 1b\n"
    "stxr w3, w2, [x1]\n"
    "cbnz w3, 1b\n"
    :: "r"(lk) : "memory"
  );
#endif

  isb();
}

