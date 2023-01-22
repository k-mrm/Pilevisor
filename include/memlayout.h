/*
 *  vmm memory layout
 */

#ifndef CORE_MEMLAYOUT_H
#define CORE_MEMLAYOUT_H

/*
 *  memory layout
 *
 *  0  - 2M : unmapped
 *  2M - 4M : fdt section
 *  4M - 1G : iomem section (mapped mmio)
 *  1G -    : vmm section (mapped vmm and heap)
 */

#define FDT_SECTION_BASE      0x200000
#define IOMEM_SECTION_BASE    0x400000
#define VMM_SECTION_BASE      0x40000000

#define FDT_SECTION_SIZE      0x200000
#define IOMEM_SECTION_SIZE    (VMM_SECTION_BASE - IOMEM_SECTION_BASE)

#ifndef __ASSEMBLER__

extern char vmm_start[], vmm_end[];
extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];
extern char __earlymem_start[], __earlymem_end[];

#define is_vmm_text(addr)     ((u64)__text_start <= (addr) && (addr) < (u64)__text_end)
#define is_vmm_rodata(addr)   ((u64)__rodata_start <= (addr) && (addr) < (u64)__rodata_end)

extern u64 pvoffset;

#define V2P(va)               ((u64)(va) - pvoffset)
#define P2V(pa)               (void *)((u64)pa + pvoffset)

#endif  /* __ASSEMBLER__ */

#endif
