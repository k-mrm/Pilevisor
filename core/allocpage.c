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

#define MAX_ORDER   10

#define ORDER_ALIGN(order, _addr)         \
  ({                                      \
    u64 addr = (u64)_addr;                \
    (((addr) + (PAGESIZE << ((order) - 1)) - 1) & ~((PAGESIZE << ((order) - 1)) - 1));    \
  })

static u64 used_bitmap[PHYSIZE >> 12 >> 6];

static void pageallocator_test(void) __unused;

struct header {
  struct header *next, *prev;
  int order;
} __packed;

struct free_chunk {
  struct header *freelist;
  int nfree;
};

static struct memzone {
  u64 start;
  u64 end;
  struct free_chunk chunks[MAX_ORDER];
  spinlock_t lock;
} mem;

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
  for(int i = order; i < MAX_ORDER; i++) {
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

  if(order > MAX_ORDER-1)
    panic("invalid order %d", order);

  spin_lock_irqsave(&mem.lock, flags);

  void *p = __alloc_pages(&mem, order);

  spin_unlock_irqrestore(&mem.lock, flags);

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

  if((u64)pages & ((PAGESIZE << order) - 1))
    panic("alignment %p %d", pages, order);

  if(order > MAX_ORDER-1)
    panic("invalid order");

  spin_lock_irqsave(&mem.lock, flags);

  __free_pages(&mem, pages, order);

  spin_unlock_irqrestore(&mem.lock, flags);
}

static void init_free_pages(struct memzone *z, void *pages, int order) {
  struct free_chunk *f = &z->chunks[order];

  freelist_add(f, pages);
  ((struct header *)pages)->order = order;
}

static void buddydump(void) {
  printf("----------buddy allocator debug----------\n");
  for(int i = 0; i < MAX_ORDER; i++) {
    struct free_chunk *f = &mem.chunks[i];

    printf("order %d %p(->%p) nfree %d\n", i, f->freelist, f->freelist->next, f->nfree);
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

void pagealloc_init_early() {
  spinlock_init(&mem.lock);

  u64 s = PAGE_ALIGN(vmm_end);
  u64 end = (u64)__earlymem_end;

  for(; s < end; s += PAGESIZE) {
    free_page((void *)s);
  }
}

void pageallocator_init() {
  /* align to PAGESIZE << (MAX_ORDER-1) */
  u64 early_alloc_end = ORDER_ALIGN(MAX_ORDER, __earlymem_end);

  struct device_node *memdev = dt_find_node_type(localnode.device_tree, "memory");
  if(!memdev)
    panic("no memory device");

  u64 reg[2];
  int rc = dt_node_propa64(memdev, "reg", reg);
  if(rc < 0)
    panic("memory device err");

  phy_end = reg[0] + reg[1];

  mem.start = early_alloc_end;
  mem.end = phy_end;

  printf("buddy: heap [%p - %p) (free area: %d MB) \n", mem.start, mem.end, (mem.end - mem.start) >> 20);
  printf("buddy: max order %d (%d MB)\n", MAX_ORDER, (PAGESIZE << (MAX_ORDER - 1)) >> 20);

  for(u64 s = mem.start; s < mem.end; s += PAGESIZE << (MAX_ORDER - 1)) {
    init_free_pages(&mem, (void *)s, MAX_ORDER - 1);
  }

  // pageallocator_test();
}
