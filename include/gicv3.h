#ifndef DRIVER_GICV3_H
#define DRIVER_GICV3_H

#include "gic.h"

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
#define ICC_SGI1R_TARGETS(v)      ((v) & 0xffff)
#define ICC_SGI1R_INTID(v)        (((v)>>24) & 0xf)
#define ICC_SGI1R_IRM(v)          (((v)>>40) & 0x1)

#define ICH_HCR_EN          (1<<0)

#define ICH_VMCR_VENG0      (1<<0)
#define ICH_VMCR_VENG1      (1<<1)

#define ICH_LR_VINTID(n)    ((n) & 0xffffffffL)
#define ICH_LR_PINTID(n)    (((n) & 0x3ffL) << 32)
#define ICH_LR_GROUP(n)     (((n) & 0x1L) << 60)
#define ICH_LR_HW           (1L << 61)
#define ICH_LR_STATE(n)     (((n) & 0x3L) << 62)

#define lr_is_inactive(lr)  (((lr >> 62) & 0x3) == LR_INACTIVE)
#define lr_is_pending(lr)   (((lr >> 62) & 0x3) == LR_PENDING)
#define lr_is_active(lr)    (((lr >> 62) & 0x3) == LR_ACTIVE)
#define lr_is_pendact(lr)   (((lr >> 62) & 0x3) == LR_PENDACT)

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

#define GITS_PIDR2          (0xffe8)

static inline u64 gicr_typer_affinity(u64 mpidr) {
  return (u64)MPIDR_AFFINITY_LEVEL0(mpidr) << 32 |
         (u64)MPIDR_AFFINITY_LEVEL1(mpidr) << 40 |
         (u64)MPIDR_AFFINITY_LEVEL2(mpidr) << 48 |
         (u64)MPIDR_AFFINITY_LEVEL3(mpidr) << 56;
}

#endif
