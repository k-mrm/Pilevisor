#ifndef NODE_H
#define NODE_H

#include "types.h"
#include "param.h"
#include "vgic.h"
#include "spinlock.h"
#include "guest.h"
#include "vm.h"
#include "net.h"
#include "vsm.h"
#include "nodectl.h"

struct node {
  struct vcpu *vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;    /* nvcpu == npcpu */

  int nodeid;

  /* virtual shared memory */
  struct vsmctl vsm;

  /* stage 2 pagetable */
  u64 *vttbr;

  /* interrupt controller */
  struct vgic *vgic;

  /* network interface card */
  struct nic *nic;

  /* internal physical address of vm's device tree file (for Linux) */
  u64 fdt_base;
  /* internal physical address of vm's initrd file (for Linux) */
  u64 initrd_base;
  /* internal physical address of vm's entrypoint */
  u64 entrypoint;

  spinlock_t lock;
  struct mmio_info *pmap;
  int npmap;

  /* node control dispatcher */
  struct nodectl *ctl;

  /* remote node */
  struct rnode_desc {
    u8 mac[6];
    u8 enabled;
  } remote[NODE_MAX];
  int nremote;
};

void node_init(struct vmconfig *vmcfg);

void pagetrap(struct node *node, u64 va, u64 size,
              int (*read_handler)(struct vcpu *, u64, u64 *, struct mmio_access *),
              int (*write_handler)(struct vcpu *, u64, u64, struct mmio_access *));

extern struct node global;

#endif
