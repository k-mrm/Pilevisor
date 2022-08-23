#ifndef NODECTL_H
#define NODECTL_H

#include "types.h"

struct nodectl {
  void (*init)(void);
  void (*initvcpu)(void);
  void (*start)(void);
  int (*register_remote_node)(u8 *);
};

void nodectl_init(void);

#endif
