#ifndef VARM_POC_VSM_H
#define VARM_POC_VSM_H

#include "types.h"
#include "param.h"
#include "memory.h"

struct vcpu;
struct node;

#define CACHE_PAGE_NUM    64

struct page_cache {

};

struct vsmctl {
  struct node_vrange local;
  struct node_vrange remotes[NODE_MAX];

  u64 *dummypgt;  /* for debug */
};

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr);
void *vsm_fetch_page(u64 page_ipa, bool wr);

void vsm_init(void);
void vsm_node_init(void);

#endif
