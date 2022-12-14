#ifndef CORE_FDT_H
#define CORE_FDT_H

#include "types.h"

typedef u16 fdt16;
typedef u32 fdt32;
typedef u64 fdt64;

struct fdt {
  union {
    struct fdthdr *hdr;
    void *data;
  };

  u32 off_dt_struct;
  u32 size_dt_struct;
  u32 off_dt_strings;
  u32 size_dt_strings;
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

struct fdt_node_header {
  fdt32 tag;      // be(FDT_BEGIN_NODE)
  char name[0];
};

struct fdt_property {
  fdt32 tag;      // be(FDT_PROP)
  fdt32 len;
  fdt32 nameoff;
  char data[0];
};

#define FDT_MAGIC     0xd00dfeed

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

static inline u64 fdt64_to_u64(fdt64 x) {
  return (x & 0xff) << 56 | ((x >> 8) & 0xff) << 48 |
          ((x >> 16) & 0xff) << 40 | ((x >> 24) & 0xff) << 32 |
          ((x >> 32) & 0xff) << 24 | ((x >> 40) & 0xff) << 16 |
          ((x >> 48) & 0xff) << 8  | ((x >> 56) & 0xff);
}

#define u64_to_fdt64  fdt64_to_u64

#define FDT_BEGIN_NODE    0x1
#define FDT_END_NODE      0x2
#define FDT_PROP          0x3
#define FDT_NOP           0x4
#define FDT_END           0x9

void fdt_probe(struct fdt *fdt, void *base);

#endif
