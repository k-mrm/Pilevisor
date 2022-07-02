#ifndef VARM_POC_VSM_H
#define VARM_POC_VSM_H

#include "types.h"
#include "param.h"
#include "memory.h"

struct node;

struct vsmctl {
  struct node_vrange local;
  struct node_vrange remotes[NODE_MAX];
  
  /* read request/reply buffer */
  char *readbuf;
  u8 finished;
};

void vsm_init(struct vsmctl *vsm);
int vsm_access(struct node *node, u64 ipa, u64 *reg, enum maccsize accsz, bool wr);

#endif
