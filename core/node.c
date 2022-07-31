#include "types.h"
#include "vcpu.h"
#include "mm.h"
#include "kalloc.h"
#include "lib.h"
#include "memmap.h"
#include "printf.h"
#include "log.h"
#include "mmio.h"
#include "node.h"

struct node global;

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

void node_init(struct vmconfig *vmcfg) {
  struct node *node = &global;

  struct guest *guest = vmcfg->guest_img;
  struct guest *fdt = vmcfg->fdt_img;
  struct guest *initrd = vmcfg->initrd_img;

  if(!guest)
    panic("guest img required");

  vmm_log("create vm `%s`\n", guest->name);
  vmm_log("n vcpu: %d\n", vmcfg->nvcpu);
  vmm_log("allocated ram: %d byte\n", vmcfg->nallocate);
  vmm_log("img_start %p img_size %p byte\n", guest->start, guest->size);
  if(fdt)
    vmm_log("fdt_start %p fdt_size %p byte\n", fdt->start, fdt->size);
  if(initrd)
    vmm_log("initrd_start %p initrd_size %p byte\n", initrd->start, initrd->size);

  if(guest->size > vmcfg->nallocate)
    panic("img_size > nallocate");
  if(vmcfg->nallocate % PAGESIZE != 0)
    panic("invalid mem size");
  if(vmcfg->nvcpu > VCPU_PER_NODE_MAX)
    panic("too many vcpu");

  node->ctl = &global_nodectl;

  /* set initrd/fdt/entrypoint ipa */
  node->initrd_base = initrd? 0x44000000 : 0;
  node->fdt_base = fdt? 0x48400000 : 0;
  node->entrypoint = vmcfg->entrypoint;

  node->nvcpu = vmcfg->nvcpu;

  u64 *vttbr = kalloc();
  if(!vttbr)
    panic("vttbr");

  /* TODO: commonize */
  u64 p, cpsize;
  /* map kernel image */
  for(p = 0; p < guest->size; p += PAGESIZE) {
    char *page = kalloc();
    if(!page)
      panic("img");

    if(guest->size - p > PAGESIZE)
      cpsize = PAGESIZE;
    else
      cpsize = guest->size - p;

    memcpy(page, (char *)guest->start+p, cpsize);
    pagemap(vttbr, vmcfg->entrypoint+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  }

  for(; p < vmcfg->nallocate; p += PAGESIZE) {
    char *page = kalloc();
    if(!page)
      panic("ram");

    pagemap(vttbr, vmcfg->entrypoint+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  }

  vmm_log("entrypoint+p %p\n", vmcfg->entrypoint+p);

  /* map initrd image */
  if(initrd)
    copy_to_guest(vttbr, node->initrd_base, (char *)initrd->start, initrd->size);
  /* map fdt image */
  if(fdt)
    copy_to_guest(vttbr, node->fdt_base, (char *)fdt->start, fdt->size);

  /* map peripheral */
  pagemap(vttbr, UARTBASE, UARTBASE, PAGESIZE, S2PTE_DEVICE|S2PTE_RW);
  pagemap(vttbr, GPIOBASE, GPIOBASE, PAGESIZE, S2PTE_DEVICE|S2PTE_RW);
  pagemap(vttbr, RTCBASE, RTCBASE, PAGESIZE, S2PTE_DEVICE|S2PTE_RW);
  pagemap(vttbr, VIRTIO0, VIRTIO0, 0x4000, S2PTE_DEVICE|S2PTE_RW);
  pagemap(vttbr, PCIE_ECAM_BASE, PCIE_ECAM_BASE, 256*1024*1024,
          S2PTE_DEVICE|S2PTE_RW);
  pagemap(vttbr, PCIE_MMIO_BASE, PCIE_MMIO_BASE, 0x2eff0000, S2PTE_DEVICE|S2PTE_RW);
  pagemap(vttbr, PCIE_HIGH_MMIO_BASE, PCIE_HIGH_MMIO_BASE, 0x100000/*XXX*/,
          S2PTE_DEVICE|S2PTE_RW);

  node->vttbr = vttbr;
  node->pmap = NULL;
  node->vgic = new_vgic(node);

  node->nic = &netdev;
  vmm_log("node mac %m\n", node->nic->mac);

  vsm_init(node);

  spinlock_init(&node->lock);

  node->ctl->initcore(node);

  node->ctl->start(node);

  /* never return here */
}

void nodedump(struct node *node) {
  printf("================== node  ================\n");
  printf("nvcpu %4d nodeid %4d\n", node->nvcpu, node->nodeid);
  printf("nic %p mac %m\n", node->nic, node->nic->mac);
  printf("fdt %p entrypoint %p\n", node->fdt_base, node->entrypoint);
  printf("ctl %p\n", node->ctl);
  printf("=========================================\n");
}
