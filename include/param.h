#ifndef CORE_PARAM_H
#define CORE_PARAM_H

/* 256 MiB per Node */
#define MEM_PER_NODE        (512*1024*1024)

/* global vm memory size = 512 MB */
#define GVM_MEMORY          (1024*1024*1024)

/* max physical cpu in this node */
#define NCPU_MAX            8

/* max vcpu per node */
#define VCPU_PER_NODE_MAX   8

/* max vcpu */
#define VCPU_MAX  4096

/* max node */
#define NODE_MAX  32

#ifndef NR_NODE
#define NR_NODE   2
#endif

#endif
