/*
 *  localnode
 */

#include "types.h"
#include "node.h"

struct localnode localnode;    /* me */

void localvm_init(int nvcpu, u64 nalloc, struct guest *guest_fdt) {
  vmm_log("node n vCPU: %d total RAM: %p byte\n", nvcpu, nalloc);

  localvm.nvcpu = nvcpu;
  localvm.nalloc = nalloc;

  localvm.pmap = NULL;
  spinlock_init(&localvm.lock);

  /* TODO: determines vm's device info from fdt file */
  (void)guest_fdt;

  s2mmu_init();
  s2mmu_init_core();

  map_guest_peripherals(localvm.vttbr);

  vgic_init();
}
