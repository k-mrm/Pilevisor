#include "aarch64.h"
#include "printf.h"
#include "panic.h"
#include "pcpu.h"
#include "vcpu.h"

static int __stacktrace(u64 sp, u64 bsp, u64 *nextsp) {
  if(sp >= (u64)mycpu->stackbase || bsp > sp)
    return -1;

  u64 x29 = *(u64 *)(sp);
  u64 x30 = *(u64 *)(sp + 8);

  printf("\tfrom: %p\n", x30 - 4);

  *nextsp = x29;

  return 0;
}

void panic(const char *fmt, ...) {
  intr_disable();

  va_list ap;
  va_start(ap, fmt);

  printf("!!!!!!vmm panic cpu%d: ", cpuid());
  vprintf(fmt, ap);
  printf("\n");

  printf("stack trace:\n");

  register const u64 current_sp asm("sp");
  u64 sp, bsp, next;
  sp = bsp = current_sp;

  while(1) {
    if(__stacktrace(sp, bsp, &next) < 0)
      break;

    sp = next;
  }

  printf("stack trace done\n");

  vcpu_dump(current);

  va_end(ap);

  for(;;)
    asm volatile("wfi");
}
