#ifndef NODECTL_H
#define NODECTL_H

#include "types.h"
#include "msg.h"

struct recv_msg;
struct nodectl {
  void (*init)(void);
  void (*initvcpu)(void);
  void (*start)(void);
};

void nodectl_init(void);

#endif
