#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include "types.h"

enum pinmode {
  INPUT = 0b000,
  OUTPUT = 0b001,
  ALT0 = 0b100,
  ALT1 = 0b101,
  ALT2 = 0b110,
  ALT3 = 0b111,
  ALT4 = 0b011,
  ALT5 = 0b010,
};

enum pullmode {
  PULLUP = 1,
  PULLDOWN = 2,
};

#endif
