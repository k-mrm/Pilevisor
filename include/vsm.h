#ifndef VARM_POC_VSM_H
#define VARM_POC_VSM_H

#include "types.h"
#include "param.h"
#include "memory.h"
#include "mm.h"
#include "spinlock.h"

struct vcpu;

#define CACHE_PAGE_NUM    64

#define CONFIG_PAGE_CACHE

/*
 *  cache page manager:
 *
 *  cache page flags
 *
 *  | ?????? | L | OOOOO | ?????????? |
 *   63    38  37 36   32 31         0
 *
 *  ?: reserved
 *  O: owner
 *  L: lock
 */

struct cache_page {
  u64 flags;
};

struct vsm_waitqueue {
  struct vsm_server_proc *head;
  struct vsm_server_proc *tail;
  spinlock_t lock;
};

struct page_desc {
  struct vsm_waitqueue *wq;
  u8 lock;
};

struct vsm_server_proc {
  struct vsm_server_proc *next;   // waitqueue
  u64 page_ipa;
  u64 copyset;        // for invalidate server
  int req_nodeid;
  int type;           // for debug
  void (*do_process)(struct vsm_server_proc *);
};

#define NR_CACHE_PAGES        (MEM_PER_NODE >> PAGESHIFT)

#define CACHE_PAGE_LOCK_BIT(p)  (((p)->flags >> 37) & 0x1)
#define CACHE_PAGE_OWNER_SHIFT  32
#define CACHE_PAGE_OWNER_MASK   0x1f
#define CACHE_PAGE_OWNER(p)     (((p)->flags >> CACHE_PAGE_OWNER_SHIFT) & CACHE_PAGE_OWNER_MASK)

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr);
void *vsm_read_fetch_page(u64 page_ipa);
void *vsm_write_fetch_page(u64 page_ipa);

void vsm_init(void);
void vsm_node_init(struct memrange *mem);

#endif
