#ifndef NODECTL_H
#define NODECTL_H

struct nodectl {
  void (*init)(void);
  void (*startcore)(void);
};

void nodectl_init(void);

#endif
