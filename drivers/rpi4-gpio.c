#include "gpio.h"
#include "device.h" 
#include "printf.h"
#include "log.h"

static void *gpio_base;

static int rpi_gpio_dt_init(struct device_node *dev) {
  const char *compat;
  u64 gpiobase, gpiolen;

  if(!dev)
    return -1;

  compat = dt_node_props(dev, "compatible");
  if(!compat)
    return -1;

  if(dt_node_prop_addr(dev, 0, &gpiobase, &gpiolen) < 0)
    return -1;

  printf("gpio: %s %p %p\n", compat, gpiobase, gpiolen);

  gpio_base = iomap(gpiobase, gpiolen);
  if(!gpio_base) {
    vmm_warn("gpio: failed to map\n");
    return -1;
  }

  return 0;
}

int gpio_init() {
  struct device_node *dev = dt_find_node_path("gpio");

  return rpi_gpio_dt_init(dev);
}
