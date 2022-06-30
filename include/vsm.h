#ifndef VARM_POC_VSM_H
#define VARM_POC_VSM_H

#include "types.h"
#include "param.h"
#include "memory.h"

struct node;

struct vsmctl {
  u64 *vttbr;
  struct node_vrange local;
  struct node_vrange remotes[NODE_MAX];
  
  /* read request/reply buffer */
  char *readbuf;
  u8 finished;
};

void vsm_init(struct vsmctl *vsm, u64 entry, u64 ram_start_gpa, u64 ram_size, u64 img_start, u64 img_size);
int vsm_access(struct node *node, u64 ipa, u64 *reg, enum maccsize accsz, bool wr);

#endif
