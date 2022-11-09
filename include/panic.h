#ifndef CORE_PANIC_H
#define CORE_PANIC_H

#include "compiler.h"

void panic(const char *fmt, ...) __noreturn;

#endif
