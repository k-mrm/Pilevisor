#ifndef DRIVER_GIC_H
#define DRIVER_GIC_H

#include "types.h"
#include "aarch64.h"

#define GIC_NSGI          16
#define GIC_SGI_MAX       15
#define GIC_NPPI          16
#define GIC_PPI_MAX       31

#define is_sgi(intid)       (0 <= (intid) && (intid) < 16)
#define is_ppi(intid)       (16 <= (intid) && (intid) < 32)
#define is_sgi_ppi(intid)   (is_sgi(intid) || is_ppi(intid))
#define is_ppi_spi(intid)   (16 <= (intid) && (intid) < 1020)
#define is_spi(intid)       (32 <= (intid) && (intid) < 1020)
#define valid_intid(intid)  (0 <= (intid) && (intid) < 1020)

#define ich_hcr_el2   arm_sysreg(4, c12, c11, 0)
#define ich_vtr_el2   arm_sysreg(4, c12, c11, 1)
#define ich_elsr_el2  arm_sysreg(4, c12, c11, 5)
#define ich_vmcr_el2  arm_sysreg(4, c12, c11, 7)
#define ich_lr0_el2   arm_sysreg(4, c12, c12, 0)
#define ich_lr1_el2   arm_sysreg(4, c12, c12, 1)
#define ich_lr2_el2   arm_sysreg(4, c12, c12, 2)
#define ich_lr3_el2   arm_sysreg(4, c12, c12, 3)
#define ich_lr4_el2   arm_sysreg(4, c12, c12, 4)
#define ich_lr5_el2   arm_sysreg(4, c12, c12, 5)
#define ich_lr6_el2   arm_sysreg(4, c12, c12, 6)
#define ich_lr7_el2   arm_sysreg(4, c12, c12, 7)
#define ich_lr8_el2   arm_sysreg(4, c12, c13, 0)
#define ich_lr9_el2   arm_sysreg(4, c12, c13, 1)
#define ich_lr10_el2  arm_sysreg(4, c12, c13, 2)
#define ich_lr11_el2  arm_sysreg(4, c12, c13, 3)
#define ich_lr12_el2  arm_sysreg(4, c12, c13, 4)
#define ich_lr13_el2  arm_sysreg(4, c12, c13, 5)
#define ich_lr14_el2  arm_sysreg(4, c12, c13, 6)
#define ich_lr15_el2  arm_sysreg(4, c12, c13, 7)

#define icc_pmr_el1       arm_sysreg(0, c4, c6, 0)
#define icc_eoir0_el1     arm_sysreg(0, c12, c8, 1)
#define icc_dir_el1       arm_sysreg(0, c12, c11, 1)
#define icc_sgi1r_el1     arm_sysreg(0, c12, c11, 5)
#define icc_iar1_el1      arm_sysreg(0, c12, c12, 0)
#define icc_eoir1_el1     arm_sysreg(0, c12, c12, 1)
#define icc_ctlr_el1      arm_sysreg(0, c12, c12, 4)
#define icc_sre_el1       arm_sysreg(0, c12, c12, 5)
#define icc_igrpen0_el1   arm_sysreg(0, c12, c12, 6)
#define icc_igrpen1_el1   arm_sysreg(0, c12, c12, 7)

#define icc_sre_el2       arm_sysreg(4, c12, c9, 5)

#define ICC_CTLR_EOImode(m) ((m) << 1)

#define ICC_SGI1R_INTID_SHIFT     24
#define ICC_SGI1R_TARGETS_MASK    0xffff
#define ICC_SGI1R_INTID(v)        (((v)>>24) & 0xf)
#define ICC_SGI1R_IRM(v)          (((v)>>40) & 0x1)

#define ICH_HCR_EN  (1<<0)

#define ICH_VMCR_VENG0  (1<<0)
#define ICH_VMCR_VENG1  (1<<1)

#define ICH_LR_VINTID(n)    ((n) & 0xffffffffL)
#define ICH_LR_PINTID(n)    (((n) & 0x3ffL) << 32)
#define ICH_LR_GROUP(n)     (((n) & 0x1L) << 60)
#define ICH_LR_HW           (1L << 61)
#define ICH_LR_STATE(n)     (((n) & 0x3L) << 62)
#define LR_INACTIVE         0L
#define LR_PENDING          1L
#define LR_ACTIVE           2L
#define LR_PENDACT          3L
#define LR_MASK             3L

#define lr_is_inactive(lr)  (((lr >> 62) & 0x3) == LR_INACTIVE)
#define lr_is_pending(lr)   (((lr >> 62) & 0x3) == LR_PENDING)
#define lr_is_active(lr)    (((lr >> 62) & 0x3) == LR_ACTIVE)
#define lr_is_pendact(lr)   (((lr >> 62) & 0x3) == LR_PENDACT)

#define GICD_CTLR           (0x0)
#define GICD_TYPER          (0x4)
#define GICD_IIDR           (0x8)
#define GICD_TYPER2         (0xc)
#define GICD_IGROUPR(n)     (0x080 + (u64)(n) * 4)
#define GICD_ISENABLER(n)   (0x100 + (u64)(n) * 4)
#define GICD_ICENABLER(n)   (0x180 + (u64)(n) * 4)
#define GICD_ISPENDR(n)     (0x200 + (u64)(n) * 4)
#define GICD_ICPENDR(n)     (0x280 + (u64)(n) * 4)
#define GICD_ISACTIVER(n)   (0x300 + (u64)(n) * 4)
#define GICD_ICACTIVER(n)   (0x380 + (u64)(n) * 4)
#define GICD_IPRIORITYR(n)  (0x400 + (u64)(n) * 4)
#define GICD_ITARGETSR(n)   (0x800 + (u64)(n) * 4)
#define GICD_ICFGR(n)       (0xc00 + (u64)(n) * 4)
#define GICD_IROUTER(n)     (0x6000 + (u64)(n) * 8)
#define GICD_PIDR2          (0xffe8)
#define GICD_PIDR2_ARCHREV(n)   (((n)>>4) & 0xf)

#define GICD_CTLR_RWP       (1 << 31)

/* Non-secure access in double security state */
#define GICD_CTLR_NS_ENGRP1     (1 << 0)
#define GICD_CTLR_NS_ENGRP1A    (1 << 1)
#define GICD_CTLR_NS_ARE_NS     (1 << 4)

/* only single security state */
#define GICD_CTLR_SS_ENGRP0     (1 << 0)
#define GICD_CTLR_SS_ENGRP1     (1 << 1)
#define GICD_CTLR_SS_ARE        (1 << 4)
#define GICD_CTLR_DS            (1 << 6)

#define GICD_TYPER_CPUNum_SHIFT   5
#define GICD_TYPER_IDbits_SHIFT   19

#define GICD_IIDR_Revision_SHIFT    12
#define GICD_IIDR_ProductID_SHIFT   24

#define GICD_PIDR2_ArchRev_SHIFT    4

#define GITS_PIDR2          (0xffe8)

#define GICRBASEn(n)        (GICRBASE+(n)*0x20000)

#define GICR_CTLR           (0x0)
#define GICR_IIDR           (0x4)
#define GICR_TYPER          (0x8)
#define GICR_WAKER          (0x14)
#define GICR_PIDR2          (0xffe8)

#define SGI_BASE  0x10000
#define GICR_IGROUPR0       (SGI_BASE+0x80)
#define GICR_ISENABLER0     (SGI_BASE+0x100)
#define GICR_ICENABLER0     (SGI_BASE+0x180)
#define GICR_ISPENDR0       (SGI_BASE+0x200)
#define GICR_ICPENDR0       (SGI_BASE+0x280)
#define GICR_ISACTIVER0     (SGI_BASE+0x300)
#define GICR_ICACTIVER0     (SGI_BASE+0x380)
#define GICR_IPRIORITYR(n)  (SGI_BASE+0x400+(n)*4)
#define GICR_ICFGR0         (SGI_BASE+0xc00)
#define GICR_ICFGR1         (SGI_BASE+0xc04)
#define GICR_IGRPMODR0      (SGI_BASE+0xd00)

#define GICR_TYPER_PLPIS        (1 << 0)
#define GICR_TYPER_VLPIS        (1 << 1)
#define GICR_TYPER_LAST         (1 << 4)
#define GICR_TYPER_DPGS         (1 << 5)
#define GICR_TYPER_PROC_NUM(p)  (((p) & 0xffff) << 8)

static inline u64 gicr_typer_affinity(u64 mpidr) {
  return (u64)MPIDR_AFFINITY_LEVEL0(mpidr) << 32 |
         (u64)MPIDR_AFFINITY_LEVEL1(mpidr) << 40 |
         (u64)MPIDR_AFFINITY_LEVEL2(mpidr) << 48 |
         (u64)MPIDR_AFFINITY_LEVEL3(mpidr) << 56;
}

enum gic_sgi {
  SGI_INJECT,
  SGI_STOP,
};

struct gic_irqchip {
  int version;      // 2 or 3
  int max_spi;
  int max_lr;

  void (*init)(void);
  void (*initcore)(void);

  u64 (*read_lr)(int n);
  void (*write_lr)(int n, u64 val);
  int (*inject_guest_irq)(u32 intid, int grp);
  bool (*irq_pending)(u32 irq);
  u32 (*read_iar)(void);
  void (*host_eoi)(u32 iar, int grp);
  void (*guest_eoi)(u32 iar, int grp);
  void (*deactive_irq)(u32 irq);
  void (*send_sgi)(int cpuid, int sgi_id);
  bool (*irq_enabled)(u32 irq);
  void (*enable_irq)(u32 irq);
  void (*disable_irq)(u32 irq);
  void (*setup_irq)(u32 irq);
  void (*set_target)(u32 irq, u8 target);
};

extern struct gic_irqchip irqchip;

struct gic_state {
  u64 lr[16];
  u64 vmcr;
  u32 sre_el1;
};

void gic_init(void);
void gic_init_cpu(void);

#endif
