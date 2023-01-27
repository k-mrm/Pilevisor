#include "gpio.h"
#include "device.h" 

static int rpi_gpio_dt_init(struct device_node *dev) {
  ;
}

int gpio_init() {
  struct device_node *node = dt_find_node_path("gpio");

  if(!node)
    return -1;

  printf("gpio init %p\n", node);

  return 0;
}
