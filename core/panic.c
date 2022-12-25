#include "aarch64.h"
#include "printf.h"
#include "panic.h"
#include "pcpu.h"
#include "vcpu.h"
#include "localnode.h"
#include "node.h"
#include "memory.h"
#include "vsm-log.h"

volatile int panicked_context = 0;

static int __stacktrace(u64 sp, u64 bsp, u64 *nextsp) {
  if(sp >= (u64)mycpu->stackbase || bsp > sp)
    return -1;

  u64 x29 = *(u64 *)(sp);
  u64 x30 = *(u64 *)(sp + 8);
  u64 callee = x30 - 4;

  if(!is_vmm_text(callee))
    return -1;

  printf("\tfrom: %p\n", x30 - 4);

  *nextsp = x29;

  return 0;
}

void panic(const char *fmt, ...) {
  isb();
  dsb(sy);

  panicked_context = 1;

  // node_panic_signal();

  local_irq_disable();

  cpu_stop_all();

  va_list ap;
  va_start(ap, fmt);

  printf("!!!!!!vmm panic Node%d:cpu%d: ", local_nodeid(), cpuid());
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);

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
  node_cluster_dump();

  vsm_logdump(100);

  for(;;) {
    wfi();
    wfe();
  }
}
