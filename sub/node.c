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
  rnode0->possible = true;
}

static int wait_for_node0_init() {
  while(!localnode.remotes[0].enabled)
    wfi();
}

static void sub_init() {
  vmm_log("sub-node init\n");

  sub_msg_init();
  vsm_node_init();

  vmm_log("waiting node 0...\n");

  intr_enable();
  wait_for_node0_init();

  vmm_log("node0 OK %m\n", localnode.remotes[0].mac);

  send_setup_done_notify(0);
}

static void sub_initvcpu() {
  load_new_local_vcpu();

  /* TODO: handle psci call (from remote) */
}

static void sub_start() {
  vmm_log("nodeN: start\n");

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
