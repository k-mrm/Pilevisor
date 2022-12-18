/*
 *  node 0(bootstrap node) controller
 */

#include "types.h"
#include "param.h"
#include "vcpu.h"
#include "localnode.h"
#include "node.h"
#include "nodectl.h"
#include "msg.h"
#include "lib.h"
#include "log.h"
#include "guest.h"
#include "arch-timer.h"

#define KiB   (1024)
#define MiB   (1024 * 1024)
#define GiB   (1024 * 1024 * 1024)

static struct vm_desc vm_desc = {
  .os_img = &linux_img,
  .fdt_img = &virt_dtb,
  .initrd_img = &rootfs_img,
  /* TODO: determine parameters by fdt file */
  .nvcpu = 2,
  .nallocate = 512 * MiB,
  .ram_start = 0x40000000,
  .entrypoint = 0x40200000,
  .fdt_base = 0x48400000,
  .initrd_base = 0x48000000,
};

static void initvm(struct vm_desc *desc) {
  struct guest *os = desc->os_img;
  struct guest *fdt = desc->fdt_img;
  struct guest *initrd = desc->initrd_img;

  if(!os)
    panic("guest-os img is required");

  if(desc->nallocate % PAGESIZE != 0)
    panic("invalid mem size");

  printf("initvm: create vm `%s`\n", os->name);
  printf("initvm: use %d vcpu(s)\n", desc->nvcpu);
  printf("initvm: allocated ram: %d (%d M) byte\n", desc->nallocate, desc->nallocate >> 20);
  printf("initvm: img_start %p img_size %p byte\n", os->start, os->size);

  map_guest_image(localvm.vttbr, os, desc->entrypoint);

  if(fdt) {
    printf("initvm: fdt_start %p fdt_size %p byte\n", fdt->start, fdt->size);
    map_guest_image(localvm.vttbr, fdt, vm_desc.fdt_base);
  } else {
    vmm_warn("initvm: fdt not found\n");
  }

  if(initrd) {
    printf("initvm: initrd_start %p initrd_size %p byte\n", initrd->start, initrd->size);
    map_guest_image(localvm.vttbr, initrd, vm_desc.initrd_base);
  } else {
    vmm_warn("initvm: initrd not found\n");
  }
}

static void node0_init_vcpu0(u64 ep, u64 fdt_base) {
  current->reg.elr = ep;
  current->reg.x[0] = fdt_base;

  current->online = true;
}

static void node0_init() {
  localnode.nodeid = 0;
  localnode.node = &cluster[0];

  cluster_init();

  initvm(&vm_desc);

  /* init vcpu0 */
  node0_init_vcpu0(vm_desc.entrypoint, vm_desc.fdt_base);
}

/* call per cpu */
static void node0_start() {
  int cpu = cpuid();
  printf("cpu%d: node0@cpu%d: start %d\n", cpu, cpu, now_cycles());

  node_cluster_dump();

  // waiting wakeup signal from vpsci
  wait_for_current_vcpu_online();

  printf("cpu%d: entry to vcpu %d\n", cpu, now_cycles());

  current->vmm_boot_clk = now_cycles();

  vcpu_entry();
}

static struct nodectl node0_ctl = {
  .init = node0_init,
  .startcore = node0_start,
};

void nodectl_init() {
  localnode.ctl = &node0_ctl;
}
