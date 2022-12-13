/*
 *  flattened device tree driver
 */

#include "types.h"
#include "fdt.h"
#include "printf.h"
#include "lib.h"
#include "panic.h"

void device_tree_init(void *fdt) {
  if(fdt_magic(fdt) != FDT_MAGIC) {
    panic("no fdt");
  }

  printf("fdt detected: version %d\n", fdt_version(fdt));
  printf("%d %d\n", fdt_off_dt_struct(fdt), fdt_off_dt_strings(fdt));

  bin_dump(fdt_off_dt_strings(fdt), 128);
}
