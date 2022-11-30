#include "types.h"
#include "malloc.h"
#include "allocpage.h"
#include "spinlock.h"
#include "mm.h"
#include "panic.h"
#include "lib.h"
#include "compiler.h"

static void __malloc_test() __unused;

static const u32 blocksize[8] = {
  16, 32, 64, 128, 248, 504, 1016, 2040
};

struct mhdr {
  struct mhdr *next;
};

struct frame {
  struct frame *next;
  struct mhdr *freelist;
};

struct chunk {
  struct frame *framelist;
};

static struct chunk malloc_chunk[8];
static spinlock_t heaplock;

static inline int chunk_order(struct chunk *c) {
  return c - malloc_chunk;
}

static int get_order(u32 size) {
  int order = 0;

  while(order < 9) {
    if(size <= blocksize[order])
      break;
    order++;
  };

  return order;
}

static struct frame *malloc_frame(int order) {
  struct frame *f = alloc_page();
  if(!f)
    return NULL;

  f->next = NULL;

  struct mhdr *freelist = NULL;

  u32 bsize = blocksize[order];

  for(u64 maddr = (u64)f + sizeof(struct frame); maddr + bsize < (u64)f + PAGESIZE; maddr += bsize) {
    ((struct mhdr *)maddr)->next = freelist;
    freelist = (struct mhdr *)maddr;
  }

  f->freelist = freelist;

  return f;
}

static void *__malloc(struct chunk *chunk, u32 size) {
  struct frame *framelist = chunk->framelist;

  if(!framelist) {   /* first call of malloc */
    framelist = chunk->framelist = malloc_frame(chunk_order(chunk));
  }

retry:
  for(struct frame *f = framelist; f; f = f->next) {
    struct mhdr *mem = f->freelist;
    if(mem) {
      f->freelist = mem->next;
      memset(mem, 0, size);
      return (void *)mem;
    }
  }

  /* add framelist */
  struct frame *newframe = malloc_frame(chunk_order(chunk));
  if(!newframe)
    return NULL;

  newframe->next = framelist;
  framelist = chunk->framelist = newframe;

  goto retry;
}

void *malloc(u32 size) {
  u64 flags = 0;

  if(size == 0)
    panic("0 malloc");

  int order = get_order(size);
  if(order >= 8)
    panic("too big: %d", size);

  struct chunk *chunk = &malloc_chunk[order];

  spin_lock_irqsave(&heaplock, flags);

  void *p = __malloc(chunk, size);

  spin_unlock_irqrestore(&heaplock, flags);

  if((u64)p & 0x7)
    panic("malloc: not aligned to 8 byte: %p", p);

  return p;
}

void free(void *ptr) {
  u64 flags = 0;

  if(!ptr)
    panic("null free");

  struct frame *frame = (struct frame *)PAGE_ADDRESS(ptr);

  spin_lock_irqsave(&heaplock, flags);

  struct mhdr *free_mem = ptr;
  free_mem->next = frame->freelist;
  frame->freelist = free_mem;

  spin_unlock_irqrestore(&heaplock, flags);
}

static void __malloc_test() {
  void *p = malloc(32);
  printf("mtest: %p\n", p);
  p = malloc(32);
  printf("mtest: %p\n", p);
  p = malloc(32);
  printf("mtest: %p\n", p);
  p = malloc(32);
  printf("mtest: %p\n", p);
  p = malloc(32);
  printf("mtest: %p\n", p);
  free(p);
  p = malloc(32);
  printf("mtest: %p\n", p);
  free(p);
}

void malloc_init() {
  spinlock_init(&heaplock);

  // __malloc_test();
}
