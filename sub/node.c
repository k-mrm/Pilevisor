#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"

/* sub node controller */

static void subnode_initcore(struct node *node) {
  vmm_log("nodeN: initcore\n");

  intr_enable();
  virtio_net_send_test();
  u32 d;
  read_sysreg(d, daif);
  printf("enabled? %p\n", d);
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();
  virtio_net_send_test();

  node->nodeid = 1;

  for(int i = 0; i < node->nvcpu; i++) {
    /* TODO: affinity */
    node->vcpus[i] = new_vcpu(node, i);
  }
}

static void subnode_start(struct node *node) {
  vmm_log("nodeN: start\n");

  intr_enable();
  enter_vcpu();
}

struct nodectl global_nodectl = {
  .initcore = subnode_initcore,
  .start = subnode_start,
};
