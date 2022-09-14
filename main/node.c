#include "types.h"
#include "param.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "msg.h"
#include "lib.h"
#include "log.h"
#include "main-msg.h"
#include "cluster.h"

/* main node (node0) controller */

static void initmem() {
  alloc_guestmem(localnode.vttbr, 0x40000000, localnode.nalloc);
}

static void initvm() {
  struct vm_desc *desc = localnode.vm_desc;

  struct guest *os = desc->os_img;
  struct guest *fdt = desc->fdt_img;
  struct guest *initrd = desc->initrd_img;

  if(!os)
    panic("guest-os img is required");

  if(desc->nallocate % PAGESIZE != 0)
    panic("invalid mem size");

  vmm_log("[vm] create vm `%s`\n", os->name);
  vmm_log("[vm] use %d vcpu(s)\n", desc->nvcpu);
  vmm_log("[vm] allocated ram: %d byte\n", desc->nallocate);
  vmm_log("[vm] img_start %p img_size %p byte\n", os->start, os->size);
  if(fdt)
    vmm_log("[vm] fdt_start %p fdt_size %p byte\n", fdt->start, fdt->size);
  else
    vmm_log("[vm] fdt not found\n");
  if(initrd)
    vmm_log("[vm] initrd_start %p initrd_size %p byte\n", initrd->start, initrd->size);
  else
    vmm_log("[vm] initrd not found\n");

  map_guest_image(localnode.vttbr, os, desc->entrypoint);
  if(fdt)
    map_guest_image(localnode.vttbr, fdt, desc->fdt_base);
  if(initrd)
    map_guest_image(localnode.vttbr, initrd, desc->initrd_base);
}

static void wait_for_init_ack() {
  // TODO: now Node 1 only
  while(!node_is_acked(1))
    wfi();
}

static void node0_init() {
  localnode.nodeid = 0;
  initmem();
  initvm();
  node0_msg_init();

  /* me */
  cluster_ack_node(localnode.nic->mac, localnode.nvcpu, localnode.nalloc);

  /* send initialization request to sub-node */
  broadcast_init_request();

  wait_for_init_ack();

  broadcast_cluster_info();
}

static void node0_initvcpu() {
  load_new_local_vcpu();

  if(current->vcpuid == 0) {
    current->reg.elr = localnode.vm_desc->entrypoint;
    current->reg.x[0] = localnode.vm_desc->fdt_base;
  } else {
    /* TODO: handle psci call */
    ;
  }
}

static void node0_start() {
  vmm_log("node0: start\n");
  cluster_dump();

  enter_vcpu();
}

static struct nodectl node0_ctl = {
  .init = node0_init,
  .initvcpu = node0_initvcpu,
  .start = node0_start,
};

void nodectl_init() {
  localnode.ctl = &node0_ctl;
}
