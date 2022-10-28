#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "lib.h"
#include "log.h"
#include "cluster.h"

/* sub node controller */

static void wait_for_acked_me() {
  vmm_log("hey %d\n", localnode.acked);
  while(localnode.acked == 0)
    wfi();

  isb();
}

static void sub_init() {
  vmm_log("sub-node init\n");
  vmm_log("Waiting for recognition from cluster...\n");

  intr_enable();

  wait_for_acked_me();

  vmm_log("Node %d initializing...\n", cluster_me()->nodeid);

  cluster_node_me_init();

  send_setup_done_notify(0);
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
