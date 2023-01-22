#include "types.h"
#include "memlayout.h"
#include "memory.h"
#include "iomem.h"
#include "panic.h"
#include "spinlock.h"
#include "mm.h"
#include "malloc.h"

#define IOMEM_NPAGES   (IOMEM_SECTION_SIZE >> PAGESHIFT)

struct ioarea {
  void *start;
  int npages;

  struct ioarea *next;
};

static struct ioarea *arealist;
static struct ioarea *usedlist;

static spinlock_t ioarea_lock = SPINLOCK_INIT;

void *iomalloc(u64 phys_addr, u64 size) {
  struct ioarea *area, *new;
  u64 offset = PAGE_OFFSET(phys_addr);
  int npages = PAGE_ALIGN(offset + size) >> PAGESHIFT;

  spin_lock(&ioarea_lock);

  for(area = arealist; area; area = area->next) {
    if(npages <= area->npages) {
      spin_unlock(&ioarea_lock);
      goto found;
    }
  }

  spin_unlock(&ioarea_lock);
  panic("nai");

found:
  new = malloc(sizeof(*new));

  new->start = area->start;
  new->npages = npages;
  new->next = usedlist;
  usedlist = new;

  if(area->npages == npages) {
    arealist = area->next;
    free(area);
  } else {
    area->start = (void *)((u64)area->start + (npages << PAGESHIFT));
    area->npages -= npages;
  }

  return (void *)((u64)new->start + offset);
}

void iofree(void *addr) {
  /* TODO */
  return;
}

void iomem_init() {
  struct ioarea *area = malloc(sizeof(*area));

  area->start = (void *)IOMEM_SECTION_BASE;
  area->npages = IOMEM_NPAGES;
  area->next = NULL;

  arealist = area;
}
