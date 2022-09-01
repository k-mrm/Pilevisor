#ifndef NODE_H
#define NODE_H

#include "types.h"
#include "param.h"
#include "vgic.h"
#include "spinlock.h"
#include "guest.h"
#include "net.h"
#include "vsm.h"
#include "nodectl.h"
#include "vcpu.h"
#include "lib.h"

struct mmio_access;

extern struct node localnode;

/* vm descriptor */
struct vm_desc {
  struct guest *os_img;
  struct guest *fdt_img;
  struct guest *initrd_img;
  int nvcpu;
  u64 ram_start;
  u64 nallocate;
  u64 entrypoint;
  u64 fdt_base;
  u64 initrd_base;
};

/* configuration per node */
struct nodeconfig {
  int nvcpu;
  u64 nallocate;
};

struct node {
  struct vcpu vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;    /* nvcpu <= npcpu */

  int nodeid;

  /* virtual shared memory */
  struct vsmctl vsm;

  /* stage 2 pagetable */
  u64 *vttbr;

  /* interrupt controller */
  struct vgic *vgic;

  /* network interface card */
  struct nic *nic;

  spinlock_t lock;
  struct mmio_info *pmap;
  int npmap;

  /* node control dispatcher */
  struct nodectl *ctl;

  struct vm_desc *vm_desc;

  u64 nalloc;

  /* remote nodes' desc */
  struct rnode_desc {
    u8 mac[6];
    u8 enabled;
  } remotes[NODE_MAX];
  int nremotes;
};

void node_preinit(int nvcpu, u64 nalloc, struct vm_desc *vm_desc);

void pagetrap(struct node *node, u64 va, u64 size,
              int (*read_handler)(struct vcpu *, u64, u64 *, struct mmio_access *),
              int (*write_handler)(struct vcpu *, u64, u64, struct mmio_access *));

static inline void remote_macaddr(int nodeid, u8 *buf) {
  if(localnode.remotes[nodeid].enabled)
    memcpy(buf, localnode.remotes[nodeid].mac, 6);
  else
    panic("uninitialized node");
}

#endif
