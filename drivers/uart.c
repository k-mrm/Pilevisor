#include "types.h"
#include "uart.h"
#include "device.h"
#include "localnode.h"

extern struct uartchip pl011;

void uart_putc(char c) {
  localnode.uart->putc(c);
}

void uart_puts(char *s) {
  localnode.uart->puts(s);
}

void uart_init() {
  struct device_node *chosen = dt_find_node_path(localnode.device_tree, "/chosen");
  struct device_node *uart = NULL;

  if(chosen) {
    const char *stdout = dt_node_props(chosen, "stdout-path");
    uart = dt_find_node_path(localnode.device_tree, stdout);
  }

  if(!uart)
    panic("no uart?");

  localnode.uart = &pl011;

  localnode.uart->init(uart);
}
