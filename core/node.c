#include "types.h"
#include "vcpu.h"
#include "mm.h"
#include "allocpage.h"
#include "lib.h"
#include "memmap.h"
#include "printf.h"
#include "log.h"
#include "mmio.h"
#include "node.h"

struct node localnode;    /* me */

void pagetrap(struct node *node, u64 ipa, u64 size,
              int (*read_handler)(struct vcpu *, u64, u64 *, struct mmio_access *),
              int (*write_handler)(struct vcpu *, u64, u64, struct mmio_access *)) {
  u64 *vttbr = node->vttbr;

  if(pagewalk(vttbr, ipa, 0))
    pageunmap(vttbr, ipa, size);

  if(mmio_reg_handler(node, ipa, size, read_handler, write_handler) < 0)
    panic("?");

  tlb_flush();
}

void node_preinit(int nvcpu, u64 nalloc, struct vm_desc *vm_desc) {
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
  localnode.vm_desc = vm_desc;

  vgic_init();

  msg_sysinit();
  vsm_init();
}

void nodedump(struct node *node) {
  printf("================== node  ================\n");
  printf("nvcpu %4d nodeid %4d\n", node->nvcpu, node->nodeid);
  printf("nic %p mac %m\n", node->nic, node->nic->mac);
  printf("ctl %p\n", node->ctl);
  printf("=========================================\n");
}
