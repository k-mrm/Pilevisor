#ifndef DRIVER_GICV2_H
#define DRIVER_GICV2_H

#include "gic.h"

#define GICD_ICPIDR2          0xfe8

#define GICD_CTLR_EnableGrp0  (1 << 0)
#define GICD_CTLR_EnableGrp1  (1 << 1)

#define GICD_ICPIDR2_ArchRev_SHIFT          4

#define GICD_SGIR_TargetList_SHIFT          16
#define GICD_SGIR_TargetListFilter_SHIFT    24

#define GICC_CTLR     0x0
#define GICC_PMR      0x4
#define GICC_BPR      0x8
#define GICC_IAR      0xc
#define GICC_EOIR     0x10
#define GICC_RPR      0x14
#define GICC_HPPIR    0x18
#define GICC_IIDR     0xfc
#define GICC_DIR      0x1000

#define GICC_CTLR_EnableGrp0      (1u << 0) 
#define GICC_CTLR_EnableGrp1      (1u << 1) 
#define GICC_CTLR_EOImode         (1u << 9)

#define GICH_HCR      0x0
#define GICH_VTR      0x4
#define GICH_VMCR     0x8
#define GICH_MISR     0x10
#define GICH_ELSR0    0x30
#define GICH_ELSR1    0x34
#define GICH_LR(n)    (0x100 + ((n) * 4))

#define GICH_HCR_EN               (1 << 0)

#define GICH_VMCR_VMG0En          (1 << 0)
#define GICH_VMCR_VMG1En          (1 << 1)

#define GICH_LR_PID_SHIFT         10
#define GICH_LR_CPUID_SHIFT       10
#define GICH_LR_Priority_SHIFT    23
#define GICH_LR_State_SHIFT       28
#define GICH_LR_Grp1              (1u << 30)
#define GICH_LR_HW                (1u << 31)

#endif
