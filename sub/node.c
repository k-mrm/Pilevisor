#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"

/* sub node controller */

static int wait_for_node0() {
  while(!localnode.remotes[0].enabled)
    wfi();
}

static void sub_init() {
  vmm_log("sub-node init");

  intr_enable();

  wait_for_node0();
}

static void sub_initvcpu() {
  load_new_local_vcpu();

  /* TODO: handle psci call (from remote) */
}

static void sub_start() {
  vmm_log("nodeN: start\n");
}

struct nodectl subnode_ctl = {
  .init = sub_init,
  .initvcpu = sub_initvcpu,
  .start = sub_start,
  .msg_recv = sub_msg_recv,
};

void nodectl_init() {
  localnode.ctl = &subnode_ctl;
}
