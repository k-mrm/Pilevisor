/*
 *  flattened device tree driver
 */

#include "types.h"
#include "fdt.h"
#include "printf.h"
#include "lib.h"
#include "device.h"
#include "panic.h"

static const char *fdt_string(struct fdt *fdt, u32 nameoff) {
  char *dt_strings = (char *)fdt->data + fdt->off_dt_strings;

  return &dt_strings[nameoff];
}

void fdt_parse(struct fdt *fdt) {
  char *dt_struct = (char *)fdt->data + fdt->off_dt_struct;
  char *dt_strings = (char *)fdt->data + fdt->off_dt_strings;

  fdt32 *cur = (fdt32 *)dt_struct;
  u32 token;

  while((token = fdt32_to_u32(*cur)) != FDT_END) {
    switch(token) {
      case FDT_BEGIN_NODE: {
        struct fdt_node_header *hdr = (struct fdt_node_header *)cur;

        printf("begin-node: %s\n", hdr->name);

        u32 nlen = strlen(hdr->name) + 1;
        int next = ((nlen + 4 - 1) & ~(4 - 1)) >> 2;

        cur += next + 1;

        break;
      }

      case FDT_END_NODE: {
        cur += 1;
        break;
      }

      case FDT_PROP: {
        struct fdt_property *prop = (struct fdt_property *)cur;

        const char *name = fdt_string(fdt, fdt32_to_u32(prop->nameoff));

        printf("name: %s data: %s %d\n", name, prop->data, fdt32_to_u32(prop->len));

        cur += 3 + (((fdt32_to_u32(prop->len) + 4 - 1) & ~(4 - 1)) >> 2);

        break;
      }

      case FDT_NOP:
        cur += 1;
        break;

      default:
        panic("fdt parser error %d", token);
    }
  }
}

void fdt_probe(struct fdt *fdt, void *base) {
  if(fdt_magic(base) != FDT_MAGIC)
    panic("no fdt");

  if(fdt_version(base) != 17)
    panic("expected version 17");

  fdt->data = fdt->hdr = base;

  fdt->off_dt_struct = fdt_off_dt_struct(base);
  fdt->off_dt_strings = fdt_off_dt_strings(base);
  fdt->size_dt_struct = fdt_size_dt_struct(base);
  fdt->size_dt_strings = fdt_size_dt_strings(base);

  bin_dump((char *)fdt->data + fdt->off_dt_struct, 128);
}
