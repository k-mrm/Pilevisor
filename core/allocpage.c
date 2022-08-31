#include "types.h"
#include "spinlock.h"
#include "log.h"
#include "mm.h"
#include "memmap.h"
#include "lib.h"
#include "param.h"
#include "allocpage.h"

#define MAX_ORDER   10

struct header {
  struct header *next;
};

extern char vmm_end[];

struct freepage {
  spinlock_t lock; 
  struct header *freelist;
};

static struct freepage freepages[MAX_ORDER];

static void *__alloc_pages(int order);

static void buddy_request(int order) {
  if(order == MAX_ORDER-1)
    return;

  void *large_page = __alloc_pages(order + 1);
  if(!large_page) {
    buddy_request(order + 1);
    large_page = __alloc_pages(order + 1);    /* retry */
  }

  u64 split_offset = PAGESIZE << order;
  free_pages(large_page, order);
  free_pages((char *)large_page + split_offset, order);
}

static void *__alloc_pages(int order) {
  struct freepage *f = &freepages[order];

  struct header *new = f->freelist;
  if(!new) {
    buddy_request(order);
    new = f->freelist;
    if(!new)
      return NULL;
  }

  f->freelist = new->next;
  memset((char *)new, 0, PAGESIZE << order);

  return new;
}

void *alloc_pages(int order) {
  if(order > MAX_ORDER-1)
    panic("invalid order");

  void *p = __alloc_pages(order);
  if(!p)
    panic("nomem");

  return p;
}

static void __free_pages(void *pages, int order) {
  struct freepage *f = &freepages[order];
  struct header *fp = (struct header *)pages;

  memset(pages, 0, PAGESIZE << order);

  fp->next = f->freelist;
  f->freelist = fp;
}

void free_pages(void *pages, int order) {
  if((u64)pages & ((PAGESIZE << order) - 1))
    panic("alignment %p", pages);
  if(order > MAX_ORDER-1)
    panic("invalid order");
  if(!pages)
    return;

  __free_pages(pages, order);
}

static void buddyinit() {
  printf("buddy %d MB byte\n", (PAGESIZE << (MAX_ORDER - 1)) / 1024 / 1024);

  u64 s = ((u64)vmm_end + (PAGESIZE << (MAX_ORDER - 1)) - 1) &
              ~((PAGESIZE << (MAX_ORDER - 1)) - 1);
  for(; s < PHYEND; s += PAGESIZE << (MAX_ORDER - 1)) {
    free_pages((void *)s, MAX_ORDER - 1);
  }
}

void pageallocator_init() {
  for(struct freepage *f = freepages; f < &freepages[MAX_ORDER]; f++) {
    spinlock_init(&f->lock);
  }

  buddyinit();
}
