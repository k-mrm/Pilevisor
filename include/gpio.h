#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#ifndef __ASSEMBLER__

#include "types.h"

enum pinmode {
  INPUT   = 0b000,
  OUTPUT  = 0b001,
  ALT0    = 0b100,
  ALT1    = 0b101,
  ALT2    = 0b110,
  ALT3    = 0b111,
  ALT4    = 0b011,
  ALT5    = 0b010,
};

enum pullmode {
  PULLUP = 1,
  PULLDOWN = 2,
};

void rpi_gpio_set_pinmode(int pin, enum pinmode mode);

#endif  /* __ASSEMBLER__ */

#define GPFSEL(n)   (0x4 * (n))
#define GPSET(n)    (0x1c + 0x4 * (n))
#define GPCLR(n)    (0x28 + 0x4 * (n))

#endif
