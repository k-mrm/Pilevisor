#ifndef CORE_PARAM_H
#define CORE_PARAM_H

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

/* global vm memory size = 768 MB */
#define GVM_MEMORY            (512*1024*1024)

/* 256 MiB per Node */
#define MEM_PER_NODE          (256*1024*1024)

/* max physical cpu in this node */
#define NCPU_MAX            4

/* max vcpu per node */
#define VCPU_PER_NODE_MAX   4

/* max vcpu */
#define VCPU_MAX  4096

/* max node */
#define NODE_MAX  32

#define NR_NODE   2

#endif
