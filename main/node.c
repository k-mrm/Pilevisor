#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "msg.h"

/* main node (node0) controller */

static int node0_init_broadcast(struct node *node0) {
  struct init_msg msg;
  init_msg_init(&msg, node0->mac);

  /* send msg */
  msg.msg.send(node0, (struct msg *)&msg);

  intr_enable();
  while(!node0->remote[1].enabled)
    wfi();

  return 0;
}

static void node0_initcore(struct node *node0) {
  vmm_log("node0: initcore\n");

  node0->nodeid = 0; 

  for(int i = 0; i < node0->nvcpu; i++) {
    node0->vcpus[i] = new_vcpu(node0, i);
  }
}

static void node0_start(struct node *node0) {
  vmm_log("node0: start\n");

  node0_init_broadcast(node0);

  enter_vcpu();
}

struct nodectl global_nodectl = {
  .initcore = node0_initcore,
  .start = node0_start,
};
