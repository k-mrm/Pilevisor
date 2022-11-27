#ifndef DRIVER_GICV2_H
#define DRIVER_GICV2_H

#include "gic.h"

#define GICC_CTLR     (0x0)
#define GICC_PMR      (0x4)
#define GICC_BPR      (0x8)
#define GICC_IAR      (0xc)
#define GICC_EOIR     (0x10)
#define GICC_RPR      (0x14)
#define GICC_HPPIR    (0x18)
#define GICC_IIDR     (0xfc)
#define GICC_DIR      (0x1000)

#define GICH_HCR      (0x0)
#define GICH_VTR      (0x4)
#define GICH_VMCR     (0x8)
#define GICH_MISR     (0x10)
#define GICH_ELSR0    (0x30)
#define GICH_ELSR1    (0x34)
#define GICH_LR(n)    (0x100 + ((n) * 4))

#endif
