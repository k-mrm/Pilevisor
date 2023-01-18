#ifndef IOMEM_H
#define IOMEM_H

#include "types.h"

void *iomalloc(u64 phys_addr, u64 size);
void iofree(void *addr);

void iomem_init(void);

#endif
