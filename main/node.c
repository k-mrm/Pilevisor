/*
 *  node 0(bootstrap node) controller
 */

#include "types.h"
#include "param.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "msg.h"
#include "lib.h"
#include "log.h"
#include "cluster.h"
#include "guest.h"

#define KiB   (1024)
#define MiB   (1024 * 1024)
#define GiB   (1024 * 1024 * 1024)

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

  map_guest_image(localnode.vttbr, os, desc->entrypoint);
}

static void wait_for_init_ack() {
  // TODO: now Node 1 only
  while(cluster_node(1)->status != NODE_ACK)
    wfi();
}

static void wait_for_sub_init_done() {
  // TODO: now Node 1 only
  while(cluster_node(1)->status != NODE_ONLINE)
    wfi();
}

static void node0_init_vcpu0(u64 ep, u64 fdt_base) {
  vcpu_initstate_core();

  current->reg.elr = ep;
  current->reg.x[0] = fdt_base;

  current->online = true;
}

static void node0_init() {
  struct vm_desc vm_desc = {
    .os_img = &linux_img,
    .fdt_img = &virt_dtb,
    .initrd_img = &rootfs_img,
    /* TODO: determine parameters by fdt file */
    .nvcpu = 2,
    .nallocate = 256 * MiB,
    .ram_start = 0x40000000,
    .entrypoint = 0x40200000,
    .fdt_base = 0x48400000,
    .initrd_base = 0x48000000,
  };

  localnode.nodeid = 0;

  /* me */
  cluster_node0_ack_node(localnode.nic->mac, localnode.nvcpu, localnode.nalloc);

  intr_enable();

  /* send initialization request to sub-node */
  node0_broadcast_init_request();
  wait_for_init_ack();

  /* broadcast cluster information to sub-node */
  node0_broadcast_cluster_info();
  wait_for_sub_init_done();

  cluster_node_me_init();

  /* init vcpu0 */
  node0_init_vcpu0(vm_desc.entrypoint, vm_desc.fdt_base);

  initvm(&vm_desc);
}

/* call per cpu */
static void node0_start() {
  int cpu = cpuid();

  vmm_log("cpu%d: node0@cpu%d: start\n", cpu, cpu);

  cluster_dump();

  wait_for_current_vcpu_online();

  vmm_log("cpu%d: entry to vcpu\n", cpu);

  vcpu_entry();
}

static struct nodectl node0_ctl = {
  .init = node0_init,
  .start = node0_start,
};

void nodectl_init() {
  localnode.ctl = &node0_ctl;
}
