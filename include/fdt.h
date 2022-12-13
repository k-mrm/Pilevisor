#ifndef CORE_FDT_H
#define CORE_FDT_H

#include "types.h"

typedef u16 fdt16;
typedef u32 fdt32;
typedef u64 fdt64;

struct device_tree {

};

struct fdthdr {
  fdt32 magic;
  fdt32 totalsize;
  fdt32 off_dt_struct;
  fdt32 off_dt_strings;
  fdt32 off_mem_rsvmap;
  fdt32 version;
  fdt32 last_comp_version;
  fdt32 boot_cpuid_phys;
  fdt32 size_dt_strings;
  fdt32 size_dt_struct;
};

#define FDT_MAGIC     0xd00dfeed

void device_tree_init(void *fdt);

#define fdt_magic(fdt)            fdt32_to_u32(((struct fdthdr *)fdt)->magic)
#define fdt_totalsize(fdt)        fdt32_to_u32(((struct fdthdr *)fdt)->totalsize)
#define fdt_off_dt_struct(fdt)    fdt32_to_u32(((struct fdthdr *)fdt)->off_dt_struct)
#define fdt_off_dt_strings(fdt)   fdt32_to_u32(((struct fdthdr *)fdt)->off_dt_strings)
#define fdt_off_mem_rsvmap(fdt)   fdt32_to_u32(((struct fdthdr *)fdt)->off_mem_rsvmap)
#define fdt_version(fdt)          fdt32_to_u32(((struct fdthdr *)fdt)->version)
#define fdt_boot_cpuid_phys(fdt)  fdt32_to_u32(((struct fdthdr *)fdt)->boot_cpuid_phys)
#define fdt_size_dt_strings(fdt)  fdt32_to_u32(((struct fdthdr *)fdt)->size_dt_strings)
#define fdt_size_dt_struct(fdt)   fdt32_to_u32(((struct fdthdr *)fdt)->size_dt_struct)

static inline u32 fdt32_to_u32(fdt32 x) {
  return (x & 0xff) << 24 | ((x >> 8) & 0xff) << 16 |
          ((x >> 16) & 0xff) << 8 | ((x >> 24) & 0xff);
}

#define u32_to_fdt32  fdt32_to_u32

#define FDT_BEGIN_NODE    0x1
#define FDT_END_NODE      0x2
#define FDT_PROP          0x3

#endif
