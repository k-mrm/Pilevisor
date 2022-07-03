#include "types.h"
#include "node.h"
#include "nodectl.h"

/* sub node controller */

static void subnode_initcore(struct node *node) {
  vmm_log("nodeN: initcore\n");
}

static void subnode_start(struct node *node) {
  vmm_log("nodeN: start\n");
}

struct nodectl global_nodectl = {
  .initcore = subnode_initcore,
  .start = subnode_start,
};
