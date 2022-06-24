#ifndef MVMM_LOG_H
#define MVMM_LOG_H

#include "printf.h"

#define vmm_log(...)  printf("[vmm-log] " __VA_ARGS__)
#define vmm_warn(...) printf("[vmm-warn] " __VA_ARGS__)

#endif
