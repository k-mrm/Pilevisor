#ifndef VSM_H
#define VSM_H

#include "types.h"
#include "param.h"
#include "memory.h"
#include "mm.h"
#include "spinlock.h"

struct vcpu;

#define CONFIG_PAGE_CACHE

/*
 *  manager page
 */
struct manager_page {
  u8 owner;
};

struct vsm_waitqueue {
  struct vsm_server_proc *head;
  struct vsm_server_proc *tail;
};

struct page_desc {
  struct vsm_waitqueue *wq;
  union {
    u16 ll;
    struct {
      /* vsm waitqueue lock */
      u8 lock;
      u8 wqlock;
    };
  };
};

struct vsm_server_proc {
  struct vsm_server_proc *next;   // waitqueue
  u64 page_ipa;
  u64 copyset;        // for invalidate server
  int req_nodeid;
  int type;           // for debug
  void (*do_process)(struct vsm_server_proc *);
};

#define NR_MANAGER_PAGES        (MEM_PER_NODE >> PAGESHIFT)

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr);
void *vsm_read_fetch_page(u64 page_ipa);
void *vsm_write_fetch_page(u64 page_ipa);

void vsm_init(void);
void vsm_node_init(struct memrange *mem);

#endif    /* VSM_H */
