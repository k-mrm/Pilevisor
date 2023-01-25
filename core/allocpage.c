/*
 *  simple buddy allocator
 */

#include "types.h"
#include "spinlock.h"
#include "log.h"
#include "mm.h"
#include "lib.h"
#include "param.h"
#include "localnode.h"
#include "allocpage.h"
#include "memory.h"
#include "memlayout.h"
#include "device.h"
#include "compiler.h"
#include "panic.h"

u64 phy_end;

#define MAX_ORDER   9

#define ORDER_ALIGN(order, _addr)         \
  ({                                      \
    u64 addr = (u64)_addr;                \
    (((addr) + (PAGESIZE << ((order) - 1)) - 1) & ~((PAGESIZE << ((order) - 1)) - 1));    \
  })

// static u64 used_bitmap[PHYSIZE >> 12 >> 6];

static void pageallocator_test(void) __unused;

struct header {
  struct header *next, *prev;
  int order;
} __packed;

struct free_chunk {
  struct header *freelist;
  int nfree;
};

struct memzone {
  u64 start;
  u64 end;
  struct free_chunk chunks[MAX_ORDER+1];
  spinlock_t lock;
};

static struct memzone memzone;

static inline u64 page_buddy_pfn(u64 pfn, int order) {
  return pfn ^ (1 << order);
}

static void freelist_add(struct free_chunk *f, void *page) {
  struct header *hp = (struct header *)page;

  hp->next = f->freelist;
  f->freelist = hp;
  f->nfree++;
}

static void expand(struct memzone *z, void *page, int order, int page_order) {
  unsigned int size = 1 << page_order;

  while(page_order > order) {
    page_order--;
    size >>= 1;

    u8 *p = (u8 *)page + (size << PAGESHIFT);
    freelist_add(&z->chunks[page_order], p);

    ((struct header *)p)->order = page_order;
  }
}

static void *__alloc_pages(struct memzone *z, int order) {
  for(int i = order; i <= MAX_ORDER; i++) {
    struct free_chunk *f = &z->chunks[i];

    if(!f->freelist)
      continue;

    struct header *p = f->freelist;
    f->freelist = p->next;
    f->nfree--;

    expand(z, p, order, i);

    return (void *)p;
  }

  /* no mem */
  return NULL;
}

void *alloc_pages(int order) {
  u64 flags = 0;

  if(order > MAX_ORDER)
    panic("invalid order %d", order);

  spin_lock_irqsave(&memzone.lock, flags);

  void *p = __alloc_pages(&memzone, order);

  spin_unlock_irqrestore(&memzone.lock, flags);

  if(!p)
    panic("nomem");

  memset(p, 0, PAGESIZE << order);

  return p;
}

static void __free_pages(struct memzone *z, void *pages, int order) {
  struct free_chunk *f = &z->chunks[order];

  memset(pages, 0, PAGESIZE << order);

  freelist_add(f, pages);
  ((struct header *)pages)->order = order;
}

void free_pages(void *pages, int order) {
  u64 flags;

  if(!pages)
    panic("null free_page");

  if(order > MAX_ORDER)
    panic("invalid order");

  if((u64)pages & ((PAGESIZE << order) - 1))
    panic("alignment %p %d", pages, order);


  spin_lock_irqsave(&memzone.lock, flags);

  __free_pages(&memzone, pages, order);

  spin_unlock_irqrestore(&memzone.lock, flags);
}

static void init_free_pages(struct memzone *z, void *pages, int order) {
  struct free_chunk *f = &z->chunks[order];

  freelist_add(f, pages);
  ((struct header *)pages)->order = order;
}

static void buddydump(void) {
  printf("----------buddy allocator debug----------\n");
  for(int i = 0; i <= MAX_ORDER; i++) {
    struct free_chunk *f = &memzone.chunks[i];

    printf("order %d %p %p(->%p) nfree %d\n",
           i, f, f->freelist, f->freelist ? f->freelist->next : NULL, f->nfree);
  }
}

static void pageallocator_test() {
  void *p = alloc_page();
  printf("alloc_page0 %p\n", p);
  free_page(p);
  p = alloc_page();
  printf("alloc_page1 %p\n", p);
  free_page(p);

  buddydump();

  p = alloc_pages(1);

  buddydump();

  p = alloc_pages(1);

  buddydump();
}

void early_allocator_init() {
  spinlock_init(&memzone.lock);

  pvoffset = VMM_SECTION_BASE - at_hva2pa(VMM_SECTION_BASE);

  // TODO: determine by fdt 
  u64 pend = V2P(vmm_end);

  pend = ALIGN_UP(pend, SZ_2MiB);

  u64 start_phys = pend;
  u64 end_phys = pend + SZ_2MiB;

  earlycon_puts("pvoffset: ");
  earlycon_putn(pvoffset);
  earlycon_puts("pend: ");
  earlycon_putn(pend);

  early_map_earlymem(start_phys, end_phys);

  u64 vstart = start_phys + VIRT_BASE;
  u64 vend = end_phys + VIRT_BASE;

  earlycon_puts("vstart: ");
  earlycon_putn(vstart);

  for(; vstart < vend; vstart += PAGESIZE) {
    free_page((void *)vstart);
  }
}

void pageallocator_init() {
  system_memory_dump();

  struct memblock *mem;
  int nslot = system_memory.nslot;
  int total = 0;

  for(mem = system_memory.slots; mem < &system_memory.slots[nslot]; mem++) {
    u64 pstart = mem->phys_start;
    u64 size = mem->size;

    for(u64 s = 0; s < size; s += PAGESIZE << MAX_ORDER) {
      if(is_reserved(pstart + s))
        continue;

      init_free_pages(&memzone, P2V(pstart + s), MAX_ORDER);

      total++;
    }
  }

  printf("total %d MB page: %d\n", (PAGESIZE << MAX_ORDER) >> 20, total);
}
