#ifndef CORE_PARAM_H
#define CORE_PARAM_H

#define VIRTIO0     0x0a000000

#define VMMBASE     0x40000000

#define PHYSIZE     (512*1024*1024)     /* 512 MB */
#define PHYEND      (VMMBASE+PHYSIZE)

/* global vm memory size = 512 MB */
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
