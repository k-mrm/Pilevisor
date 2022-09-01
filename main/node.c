#include "types.h"
#include "param.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "msg.h"
#include "lib.h"
#include "log.h"
#include "main-msg.h"
#include "cnt.h"

/* main node (node0) controller */

static int node0_init_broadcast() {
  struct init_req req;
  init_req_init(&req, localnode.nic->mac);

  msg_send(req);

  return 0;
}

void node0_register_remote(u8 *remote_mac) {
  u8 idx = ++localnode.nremotes;
  if(idx >= NODE_MAX) 
    panic("remote node");

  struct rnode_desc *rnode = &localnode.remotes[idx];

  memcpy(rnode->mac, remote_mac, 6);
  rnode->enabled = 1;
}

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

static void node0_init() {
  initmem();
  initvm();
}

static void node0_initvcpu() {
  load_new_local_vcpu();

  if(current->cpuid == 0) {
    current->reg.elr = localnode.vm_desc->entrypoint;
    current->reg.x[0] = localnode.vm_desc->fdt_base;
  } else {
    /* TODO: handle psci call */
    ;
  }
}

static void node0_start() {
  vmm_log("node0: start\n");

  node0_init_broadcast();

  intr_enable();

  // TODO: now Node 1 only
  while(!localnode.remotes[1].enabled)
    wfi();

  vmm_log("node1 ok\n");

  enter_vcpu();
}

static struct nodectl node0_ctl = {
  .init = node0_init,
  .initvcpu = node0_initvcpu,
  .start = node0_start,
  .msg_recv_intr = node0_msg_recv_intr,
};

void nodectl_init() {
  localnode.ctl = &node0_ctl;
}
