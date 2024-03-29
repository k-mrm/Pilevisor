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

struct device_node *fdt_parse(struct fdt *fdt) {
  char *dt_struct = (char *)fdt->data + fdt->off_dt_struct;

  fdt32 *cur = (fdt32 *)dt_struct;
  int depth = 0;
  u32 token;

  struct device_node *node = NULL;
  struct device_node *root = NULL;

  while((token = fdt32_to_u32(*cur)) != FDT_END) {
    switch(token) {
      case FDT_BEGIN_NODE: {
        struct fdt_node_header *hdr = (struct fdt_node_header *)cur;

        node = dt_node_alloc(node);
        if(!root)
          root = node;

        depth++;

        node->name = hdr->name;

        u32 nlen = strlen(hdr->name) + 1;
        int next = ((nlen + 4 - 1) & ~(4 - 1)) >> 2;

        cur += next + 1;

        break;
      }

      case FDT_END_NODE: {
        if(depth-- == 0)
          panic("depth");

        node = node->parent;

        cur += 1;

        break;
      }

      case FDT_PROP: {
        struct fdt_property *prop = (struct fdt_property *)cur;

        struct property *p = dt_prop_alloc(node);

        const char *name = fdt_string(fdt, fdt32_to_u32(prop->nameoff));

        p->name = name;
        p->data = prop->data;
        p->data_len = fdt32_to_u32(prop->len);

        if(strcmp(p->name, "device_type") == 0) {
          node->device_type = (const char *)p->data;
        } else if(strcmp(p->name, "phandle") == 0) {
          node->phandle = fdt32_to_u32(*(u32 *)p->data);
        }

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

  if(node != NULL)
    panic("?");

  return root;
}

static bool fdt_node_name_eq() {
  return false;
}

static u64 fdt_next_node(u64 offset) {
  return 0;
}

static u64 fdt_offset_path(const void *fdt, const char *path) {
  /* alias unsupported */
  if(*path != '/')
    return 0;

  while(*path == '/')
    path++;

  u64 offset;
  int rc;

  for(offset = 0; ; offset = fdt_next_node(offset)) {
    ;
  }
}

static fdt32 *fdt_get_prop(const void *fdt, u64 offset, const char *name, u32 *plen) {
  ;
}

static void early_fdt_memory_init(const void *fdt) {
  u64 memory = fdt_offset_path(fdt, "/memory");
  fdt32 *reg;
  u32 len;

  if(!memory)
    return;

  reg = fdt_get_prop(fdt, memory, "reg", &len);
}

void early_fdt_init(const void *fdt_phys) {
  if(fdt_magic(fdt_phys) != FDT_MAGIC)
    return;

  if(fdt_version(fdt_phys) != 17)
    return;

  early_fdt_memory_init(fdt_phys);
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
}
