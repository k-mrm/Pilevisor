#include "types.h"
#include "uart.h"
#include "device.h"
#include "localnode.h"

static const struct dt_device sentinel __used __section("__dt_serial_sentinel");

extern struct uartchip pl011;

void uart_putc(char c) {
  localnode.uart->putc(c);
}

void uart_puts(char *s) {
  localnode.uart->puts(s);
}

void uart_init() {
  struct device_node *n;
  struct dt_device *dev;
  int rc;

  for(n = next_match_node(__dt_serial_device, &dev, NULL); n;
      n = next_match_node(__dt_serial_device, &dev, n)) {
    if(!dev) {
      printf("uart: dev?\n");
      continue;
    }

    if(dev->init) {
      if(dev->init(n) == 0)
        break;
    }
  }

  if(!n)
    panic("no serial device");
  if(!localnode.uart)
    panic("uart???");
}
