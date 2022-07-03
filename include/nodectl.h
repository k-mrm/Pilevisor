#ifndef NODECTL_H
#define NODECTL_H

#include "types.h"

struct node;

struct nodectl {
  void (*initcore)(struct node *);
  void (*start)(struct node *);
};

extern struct nodectl global_nodectl;

#endif
