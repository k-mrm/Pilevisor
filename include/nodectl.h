#ifndef NODECTL_H
#define NODECTL_H

#include "types.h"

struct node;

struct nodectl {
  void (*initcore)(struct node *);
  void (*start)(struct node *);
  int (*register_remote_node)(struct node *, u8 *);
};

extern struct nodectl global_nodectl;

#endif
