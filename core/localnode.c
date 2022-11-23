/*
 *  localnode
 */

#include "types.h"
#include "node.h"

struct localnode localnode;    /* me */

void localnode_preinit(int nvcpu, u64 nalloc, struct guest *guest_fdt) {
  vmm_log("node n vCPU: %d total RAM: %p byte\n", nvcpu, nalloc);

  u64 *vttbr = alloc_page();
  if(!vttbr)
    panic("vttbr");

  localnode.vttbr = vttbr;
  write_sysreg(vttbr_el2, vttbr);

  map_peripherals(localnode.vttbr);

  localnode.nvcpu = nvcpu;
  localnode.nalloc = nalloc;

  localnode.pmap = NULL;
  spinlock_init(&localnode.lock);

  /* TODO: determines vm's device info from fdt file */
  (void)guest_fdt;

  vgic_init();

  msg_sysinit();
}
