/*
 *  sub node controller
 */

#include "types.h"
#include "vcpu.h"
#include "localnode.h"
#include "node.h"
#include "nodectl.h"
#include "lib.h"
#include "log.h"
#include "guest.h"

#define KiB   (1024)
#define MiB   (1024 * 1024)
#define GiB   (1024 * 1024 * 1024)

static void sub_init() {
  vmm_log("sub-node init\n");

  subnode_cluster_init();
}

static void sub_start() {
  vmm_log("node%d@cpu%d: start\n", localnode.nodeid, cpuid());

  vcpu_initstate_core();

  wait_for_current_vcpu_online();

  vcpu_entry();
}

struct nodectl subnode_ctl = {
  .init = sub_init,
  .startcore = sub_start,
};

void nodectl_init() {
  localnode.ctl = &subnode_ctl;
}
