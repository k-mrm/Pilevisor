#ifndef CORE_UART_H
#define CORE_UART_H

#include "types.h"

struct uartchip {
  char *name;
  int intid;
  void (*init)(void);
  void (*putc)(char c);
  void (*puts)(char *s);
};

void uart_putc(char c);
void uart_puts(char *s);

void uart_init(void);

#endif
