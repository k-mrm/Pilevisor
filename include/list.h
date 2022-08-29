#ifndef UTIL_LIST_H
#define UTIL_LIST_H

#include "types.h"

struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

#endif
