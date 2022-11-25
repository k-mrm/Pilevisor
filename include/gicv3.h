#ifndef DRIVER_GICV3_H
#define DRIVER_GICV3_H

#include "gic.h"

static inline u32 gicd_r(u32 offset) {
  return *(volatile u32 *)(u64)(GICDBASE + offset);
}

static inline void gicd_w(u32 offset, u32 val) {
  *(volatile u32 *)(u64)(GICDBASE + offset) = val;
}

static inline u32 gicr_r32(int cpuid, u32 offset) {
  return *(volatile u32 *)(u64)(GICRBASEn(cpuid) + offset);
}

static inline void gicr_w32(int cpuid, u32 offset, u32 val) {
  *(volatile u32 *)(u64)(GICRBASEn(cpuid) + offset) = val;
}

static inline u64 gicr_r64(int cpuid, u32 offset) {
  return *(volatile u64 *)(u64)(GICRBASEn(cpuid) + offset);
}

static inline void gicr_w64(int cpuid, u32 offset, u32 val) {
  *(volatile u64 *)(u64)(GICRBASEn(cpuid) + offset) = val;
}

#endif
