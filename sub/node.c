#include "types.h"
#include "vcpu.h"
#include "node.h"
#include "nodectl.h"

/* sub node controller */

static void initmem(struct node *node) {
  ;
}

static void subnode_init(struct nodeconfig *ndcfg) {
  vmm_log("sub-node init");
}

static void subnode_start(void) {
  vmm_log("nodeN: start\n");

  intr_enable();
  enter_vcpu();
}

struct nodectl subnode_ctl = {
  .initcore = subnode_initcore,
  .start = subnode_start,
};

void nodectl_init() {
  localnode.ctl = &subnode_ctl;
}
