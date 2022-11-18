#include "types.h"
#include "param.h"
#include "aarch64.h"
#include "mmio.h"
#include "memmap.h"
#include "vcpu.h"
#include "node.h"
#include "log.h"
#include "spinlock.h"
#include "mm.h"
#include "malloc.h"
#include "panic.h"

static struct mmio_region *alloc_mmio_region(struct mmio_region *prev) {
  struct mmio_region *m = malloc(sizeof(*m));

  m->next = prev;

  return m;
}

int mmio_emulate(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct mmio_region *map = localnode.pmap;
  int c = -1;
  u64 ipa = mmio->ipa;

  for(struct mmio_region *m = map; m; m = m->next) {
    if(m->base <= ipa && ipa < m->base + m->size) {
      mmio->offset = ipa - m->base;
      if(mmio->wnr && m->write)
        c = m->write(vcpu, mmio);
      else if(m->read)
        c = m->read(vcpu, mmio);
      break;
    }
  }

  return c;
}

int mmio_reg_handler(u64 ipa, u64 size,
                     int (*read)(struct vcpu *, struct mmio_access *),
                     int (*write)(struct vcpu *, struct mmio_access *)) {
  if(size == 0)
    return -1;

  spin_lock(&localnode.lock);

  struct mmio_region *new = alloc_mmio_region(localnode.pmap);
  localnode.pmap = new;

  new->base = ipa;
  new->size = size;
  new->read = read;
  new->write = write;

  spin_unlock(&localnode.lock);

  return 0;
}

void mmio_init() {
  spinlock_init(&localnode.lock);
}
