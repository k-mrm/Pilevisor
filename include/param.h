#ifndef CORE_PARAM_H
#define CORE_PARAM_H

#define GICDBASE    0x08000000
#define GICCBASE    0x08010000
#define GICHBASE    0x08030000
#define GICVBASE    0x08040000
#define GITSBASE    0x08080000
#define GICRBASE    0x080a0000
#define UARTBASE    0x09000000
#define RTCBASE     0x09010000
#define GPIOBASE    0x09030000
#define VIRTIO0     0x0a000000

#define PCIE_MMIO_BASE       0x10000000
#define PCIE_HIGH_MMIO_BASE  0x8000000000ULL
#define PCIE_ECAM_BASE       0x4010000000ULL

#define VMMBASE     0x40000000

#define PHYSIZE     (512*1024*1024)     /* 512 MB */
#define PHYEND      (VMMBASE+PHYSIZE)

#define GVM_MEMORY  (512*1024*1024)     /* global vm memory size = 512 MB */

/* n physical cpu in this node */
#define NCPU      1

/* max vcpu per node */
#define VCPU_PER_NODE_MAX   4

/* max vcpu */
#define VCPU_MAX  4096

/* max node */
#define NODE_MAX  32

#define NR_NODE   2

#endif
