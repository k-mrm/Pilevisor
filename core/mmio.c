#include "types.h"
#include "param.h"
#include "aarch64.h"
#include "mmio.h"
#include "memmap.h"
#include "vcpu.h"
#include "node.h"
#include "log.h"
#include "spinlock.h"

static struct mmio_info mmio_infos[128];
static spinlock_t mi_lock;

static struct mmio_info *alloc_mmio_info(struct mmio_info *prev) {
  acquire(&mi_lock);

  for(struct mmio_info *m = mmio_infos; m < &mmio_infos[128]; m++) {
    if(m->size == 0) {
      m->size = 1;
      m->next = prev;
      release(&mi_lock);
      return m;
    }
  }

  release(&mi_lock);

  panic("nomem");
  return NULL;
}

int mmio_emulate(struct vcpu *vcpu, int rn, struct mmio_access *mmio) {
  struct mmio_info *map = localnode.pmap;

  u64 ipa = mmio->ipa;
  u64 *reg = NULL;
  u64 val;

  if(rn == 31) {
    /* x31 == xzr */
    val = 0;
  } else {
    reg = &vcpu->reg.x[rn];
    val = *reg;
  }

  for(struct mmio_info *m = map; m; m = m->next) {
    if(m->base <= ipa && ipa < m->base + m->size) {
      if(mmio->wnr && m->write)
        return m->write(vcpu, ipa - m->base, val, mmio);
      else if(m->read)
        return m->read(vcpu, ipa - m->base, reg, mmio);
      else
        return -1;
    }
  }

  return -1;
}

int mmio_reg_handler(struct node *node, u64 ipa, u64 size,
                     int (*read)(struct vcpu *, u64, u64 *, struct mmio_access *),
                     int (*write)(struct vcpu *, u64, u64, struct mmio_access *)) {
  if(size == 0)
    return -1;

  acquire(&node->lock);
  struct mmio_info *new = alloc_mmio_info(node->pmap);
  node->pmap = new;
  release(&node->lock);

  new->base = ipa;
  new->size = size;
  new->read = read;
  new->write = write;

  return 0;
}
