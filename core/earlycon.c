#include "types.h"
#include "earlycon.h"
#include "pl011.h"

static inline u32 r(u64 addr) {
  return *(volatile u32 *)addr;
}

static inline void w(u64 addr, u32 val) {
  *(volatile u32 *)addr = val;
}

static inline void earlycon_putc(char c) {
  while(r(EARLY_PL011_BASE + FR) & FR_TXFF)
    ;

  w(EARLY_PL011_BASE + DR, c);
}

void earlycon_puts(const char *s) {
  char c;

  while((c = *s++))
    earlycon_putc(c);
}
