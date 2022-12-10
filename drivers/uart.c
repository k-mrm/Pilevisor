#include "types.h"
#include "uart.h"
#include "localnode.h"

extern struct uartchip pl011;

void uart_putc(char c) {
  localnode.uart->putc(c);
}

void uart_puts(char *s) {
  localnode.uart->puts(s);
}

void uart_init() {
  localnode.uart = &pl011;

  localnode.uart->init();
}
