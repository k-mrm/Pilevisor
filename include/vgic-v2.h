#ifndef VGIC_V2_H
#define VGIC_V2_H

#include "vgic.h"
#include "types.h"

void vgic_v2_init(struct vgic *vgic);
void vgic_v2_pre_init(physaddr_t vbase);
void vgic_v2_virq_set_target(struct vgic_irq *virq, u64 vcpuid);

#endif
