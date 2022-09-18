#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "lib.h"
#include "log.h"
#include "cluster.h"

/* sub node controller */

static int wait_for_acked_me() {
  isb();
  vmm_log("hey %d", localnode.acked);
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

  vsm_node_init(&cluster_me()->mem);
  vcpuid_init(cluster_me()->vcpus, cluster_me()->nvcpu);

  send_setup_done_notify(0);
}

static void sub_initvcpu() {
  load_new_local_vcpu();

  /* TODO: handle psci call (from remote) */
}

static void sub_start() {
  vmm_log("node%d: start\n", localnode.nodeid);

  intr_enable();

  for(;;)
    wfi();
}

struct nodectl subnode_ctl = {
  .init = sub_init,
  .initvcpu = sub_initvcpu,
  .start = sub_start,
};

void nodectl_init() {
  localnode.ctl = &subnode_ctl;
}
