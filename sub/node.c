#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "sub-msg.h"
#include "lib.h"
#include "log.h"

/* sub node controller */

void sub_register_node0(u8 *node0_mac) {
  struct rnode_desc *rnode0 = &localnode.remotes[0];
  memcpy(rnode0->mac, node0_mac, 6);
  rnode0->enabled = 1;
}

static int wait_for_node0_init() {
  while(!localnode.remotes[0].enabled)
    wfi();
}

static void sub_init() {
  vmm_log("sub-node init");

  intr_enable();

  wait_for_node0_init();
  vmm_log("node0 OK\n");
}

static void sub_initvcpu() {
  load_new_local_vcpu();

  /* TODO: handle psci call (from remote) */
}

static void sub_start() {
  vmm_log("nodeN: start\n");

  panic("hi");
}

struct nodectl subnode_ctl = {
  .init = sub_init,
  .initvcpu = sub_initvcpu,
  .start = sub_start,
  .msg_recv_intr = sub_msg_recv_intr,
};

void nodectl_init() {
  localnode.ctl = &subnode_ctl;
}
