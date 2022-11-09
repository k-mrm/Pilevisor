/*
 *  simple buddy allocator
 */

#include "types.h"
#include "spinlock.h"
#include "log.h"
#include "mm.h"
#include "memmap.h"
#include "lib.h"
#include "param.h"
#include "allocpage.h"
#include "compiler.h"
#include "panic.h"

#define MAX_ORDER   10

struct header {
  struct header *next;
};

extern char vmm_end[];

struct free_chunk {
  spinlock_t lock; 
  struct header *freelist;
};

static struct free_chunk free_chunks[MAX_ORDER];

static void *__alloc_pages(int order);

/*
 *  TODO: consider spinlock
 */

static void *buddy_request_page(int order) {
  if(order == MAX_ORDER-1)
    return NULL;

  void *large_page = __alloc_pages(order + 1);
  if(!large_page) {
    large_page = buddy_request_page(order + 1);
    if(!large_page)
      return NULL;
  }

  u64 split_offset = PAGESIZE << order;
  free_pages(large_page, order);

  return (char *)large_page + split_offset;
}

static void *__alloc_pages(int order) {
  struct free_chunk *f = &free_chunks[order];

  struct header *new = f->freelist;
  if(new)
    goto get_from_freelist;
  
  new = buddy_request_page(order);
  if(new)
    goto get_pages;

  return NULL;

get_from_freelist:
  f->freelist = new->next;

get_pages:
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
  struct free_chunk *f = &free_chunks[order];
  struct header *fp = (struct header *)pages;

  memset(pages, 0, PAGESIZE << order);

  fp->next = f->freelist;
  f->freelist = fp;
}

void free_pages(void *pages, int order) {
  if(!pages)
    return;

  if((u64)pages & ((PAGESIZE << order) - 1))
    panic("alignment %p", pages);

  if(order > MAX_ORDER-1)
    panic("invalid order");

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

__unused
static void pageallocator_test() {
  void *a[100];

  for(int i = 0; i < 100; i++) {
    a[i] = alloc_page();
    printf("pageallocator %p\n", a[i]);
  }

  for(int i = 0; i < 100; i++) {
    free_page(a[i]);
    printf("frreeeeing %p\n", a[i]);
  }

  for(int i = 0; i < 100; i++) {
    a[i] = alloc_page();
    printf("pageallocator %p\n", a[i]);
  }

  for(int i = 0; i < 100; i++) {
    free_page(a[i]);
    printf("frreeeeing %p\n", a[i]);
  }
}

void pageallocator_init() {
  for(struct free_chunk *f = free_chunks; f < &free_chunks[MAX_ORDER]; f++) {
    spinlock_init(&f->lock);
  }

  buddyinit();
}
