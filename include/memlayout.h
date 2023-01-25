/*
 *  vmm memory layout
 */

#ifndef CORE_MEMLAYOUT_H
#define CORE_MEMLAYOUT_H

/*
 *  memory layout
 *
 *  0  - 2M : unmapped
 *  2M - 6M : fdt section
 *  6M - 2G : iomem section (mapped mmio)
 *  2G - 3G : vmm kernel
 *  3G -    : memory linear map
 */

#define FDT_SECTION_BASE      0x200000
#define IOMEM_SECTION_BASE    0x600000
#define VMM_SECTION_BASE      0x80000000
#define VIRT_BASE             0xc0000000

#define FDT_SECTION_SIZE      0x400000
#define IOMEM_SECTION_SIZE    (VMM_SECTION_BASE - IOMEM_SECTION_BASE)

#ifndef __ASSEMBLER__

extern char vmm_start[], vmm_end[];
extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];

#define is_vmm_text(addr)     ((u64)__text_start <= (addr) && (addr) < (u64)__text_end)
#define is_vmm_rodata(addr)   ((u64)__rodata_start <= (addr) && (addr) < (u64)__rodata_end)

extern u64 pvoffset;

static inline bool is_linear_addr(u64 va) {
  return va >= VIRT_BASE;
}

static inline u64 virt2phys(u64 va) {
  return is_linear_addr(va) ? va - VIRT_BASE : va - pvoffset;
}

static inline void *phys2virt(u64 pa) {
  return (void *)(pa + VIRT_BASE);
}

static inline u64 phys2kern(u64 pa) {
  return pa + pvoffset;
}

#define V2P(va)               virt2phys((u64)va)
#define P2V(pa)               phys2virt((u64)pa)

#endif  /* __ASSEMBLER__ */

#endif
