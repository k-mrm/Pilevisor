#include "types.h"
#include "node.h"
#include "nodectl.h"

/* main node (node0) controller */

static void node0_initcore(struct node *node0) {
  vmm_log("node0: initcore\n");
}

static void node0_start(struct node *node0) {
  vmm_log("node0: start\n");
}

struct nodectl global_nodectl = {
  .initcore = node0_initcore,
  .start = node0_start,
};
