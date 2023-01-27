#include "gpio.h"
#include "device.h" 
#include "printf.h"
#include "log.h"
#include "mm.h"

static void *gpio_base;

static void gpio_write(u32 offset, u32 val) {
  *(volatile u32 *)((u64)gpio_base + offset) = val;
}

static u32 gpio_read(u32 offset) {
  return *(volatile u32 *)((u64)gpio_base + offset);
}

void rpi_gpio_set(int pin) {
  int ch = pin / 32;
  int n = pin % 32;
  u32 gp;

  gp = gpio_read(GPSET(ch));

  gpio_write(GPSET(ch), gp | (1 << n));
}

void rpi_gpio_clr(int pin) {
  int ch = pin / 32;
  int n = pin % 32;
  u32 gp;

  gp = gpio_read(GPCLR(ch));

  gpio_write(GPCLR(ch), gp | (1 << n));
}

void rpi_gpio_set_pinmode(int pin, enum pinmode mode) {
  int ch = pin / 10;
  int n = pin % 10 * 3;

  u32 gpf = gpio_read(GPFSEL(ch));

  gpf &= ~(7 << n);
  gpf |= mode << n;

  gpio_write(GPFSEL(ch), gpf);
}

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
  if(!dev)
    return -1;

  return rpi_gpio_dt_init(dev);
}
