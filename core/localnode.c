/*
 *  localnode
 */

#include "types.h"
#include "node.h"
#include "s2mm.h"
#include "param.h"
#include "vcpu.h"
#include "arch-timer.h"

struct localnode localnode = {0};    /* me */

void localvm_initcore() {
  vcpu_init_core();
}

void setup_node0_bootclock() {
  if(current == vcpu0)
    localnode.bootclk = now_cycles();
}

void localvm_init(int nvcpu, u64 nalloc, struct guest *guest_fdt) {
  printf("this node vCPU: %d total RAM: %p byte\n", nvcpu, nalloc);

  localvm.nvcpu = nvcpu;
  localvm.nalloc = nalloc;

  if(localvm.nvcpu > NCPU_MAX)
    panic("too vcpu");
  if(localvm.nalloc != MEM_PER_NODE)
    panic("localvm.nalloc != MEM_PER_NODE %p", MEM_PER_NODE);

  localvm.pmap = NULL;
  spinlock_init(&localvm.lock);

  /* TODO: determines vm's device info from fdt file */
  (void)guest_fdt;

  vcpu_preinit();

  s2mmu_init();
  map_guest_peripherals();
  vgic_init();

  vgic_connect_hwirq(153, 153);
}
