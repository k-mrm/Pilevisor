#include "types.h"
#include "device.h"
#include "fdt.h"

struct device_tree *device_tree_init(void *fdt_base) {
  struct fdt fdt;

  fdt_probe(&fdt, fdt_base);

  fdt_parse(&fdt);
}
