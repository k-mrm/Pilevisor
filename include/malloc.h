#ifndef MALLOC_H
#define MALLOC_H

#include "types.h"

void *malloc(u32 size);
void free(void *ptr);

void malloc_init(void);

#endif
