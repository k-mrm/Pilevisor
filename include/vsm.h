#ifndef VARM_POC_VSM_H
#define VARM_POC_VSM_H

#include "types.h"
#include "param.h"
#include "memory.h"

struct vcpu;
struct node;

struct vsmctl {
  struct node_vrange local;
  struct node_vrange remotes[NODE_MAX];

  u64 *dummypgt;
  
  /* read request/reply buffer */
  char *readbuf;
  u8 finished;
};

int vsm_fetch_pagetable(struct node *node, u64 page_ipa);
int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr);
void vsm_node_init(void);

#endif
