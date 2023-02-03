#ifndef VGIC_V3_H
#define VGIC_V3_H

#include "vgic.h"
#include "types.h"

void vgic_v3_virq_set_target(struct vgic_irq *virq, u64 vcpuid);

#endif
