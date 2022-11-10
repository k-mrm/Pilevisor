/*
 *  sub node controller
 */

#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "lib.h"
#include "log.h"
#include "cluster.h"
#include "guest.h"

#define KiB   (1024)
#define MiB   (1024 * 1024)
#define GiB   (1024 * 1024 * 1024)

static void wait_for_acked_me() {
  vmm_log("hey %d\n", localnode.acked);
  while(localnode.acked == 0)
    wfi();

  isb();
}

static void ap_initvm() {
  struct vm_desc vm_desc = {
    .os_img = NULL,
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

  struct guest *fdt = vm_desc.fdt_img;
  struct guest *initrd = vm_desc.initrd_img;

  if(fdt) {
    printf("initvm: fdt_start %p fdt_size %p byte\n", fdt->start, fdt->size);
    map_guest_image(localnode.vttbr, fdt, vm_desc.fdt_base);
  } else {
    vmm_warn("initvm: fdt not found\n");
  }

  if(initrd) {
    printf("initvm: initrd_start %p initrd_size %p byte\n", initrd->start, initrd->size);
    map_guest_image(localnode.vttbr, initrd, vm_desc.initrd_base);
  } else {
    vmm_warn("initvm: initrd not found\n");
  }
}

static void sub_init() {
  vmm_log("sub-node init\n");
  vmm_log("Waiting for recognition from cluster...\n");

  intr_enable();

  wait_for_acked_me();

  vmm_log("Node %d initializing...\n", cluster_me()->nodeid);

  cluster_node_me_init();

  send_setup_done_notify(0);

  ap_initvm();
}

static void sub_start() {
  vmm_log("node%d@cpu%d: start\n", localnode.nodeid, cpuid());

  vcpu_initstate_core();

  wait_for_current_vcpu_online();

  vcpu_entry();
}

struct nodectl subnode_ctl = {
  .init = sub_init,
  .start = sub_start,
};

void nodectl_init() {
  localnode.ctl = &subnode_ctl;
}
