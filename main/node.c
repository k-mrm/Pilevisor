#include "types.h"
#include "param.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"
#include "msg.h"
#include "lib.h"

/* main node (node0) controller */

static int node0_init_broadcast(struct node *node0) {
  struct init_msg msg;
  init_msg_init(&msg, node0->mac);

  /* send msg */
  msg.msg.send(node0, (struct msg *)&msg);

  intr_enable();
  while(!node0->remote[1].enabled)
    wfi();

  printf("node1 ok\n");

  return 0;
}

static void node0_initcore(struct node *node0) {
  vmm_log("node0: initcore\n");

  node0->nodeid = 0; 

  for(int i = 0; i < node0->nvcpu; i++) {
    node0->vcpus[i] = new_vcpu(node0, i);
  }

  vcpu_ready(node0->vcpus[0]);
}

static void node0_start(struct node *node0) {
  vmm_log("node0: start\n");

  node0_init_broadcast(node0);

  enter_vcpu();
}

static int node0_register_remote(struct node *node0, u8 *remote_mac) {
  u8 idx = ++node0->nremote;
  if(idx > NODE_MAX) 
    panic("remote node");

  struct rnode_desc *rnode = &node0->remote[idx];

  memcpy(rnode->mac, remote_mac, sizeof(u8)*6);

  rnode->enabled = 1;

  return 0;
}

struct nodectl global_nodectl = {
  .initcore = node0_initcore,
  .start = node0_start,
  .register_remote_node = node0_register_remote,
};
