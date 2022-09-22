#ifndef MVMM_PSCI_H
#define MVMM_PSCI_H

#include "types.h"

#define PSCI_VERSION            0x84000000
#define PSCI_MIGRATE_INFO_TYPE  0x84000006
#define PSCI_SYSTEM_OFF         0x84000008
#define PSCI_SYSTEM_RESET       0x84000009
#define PSCI_SYSTEM_FEATURES    0x8400000a
#define PSCI_SYSTEM_CPUON       0xc4000003

#define PSCI_SUCCESS            0
#define PSCI_NOT_SUPPORTED      -1
#define PSCI_INVALID_PARAMETERS -2
#define PSCI_DENIED             -3
#define PSCI_ALREADY_ON         -4
#define PSCI_ON_PENDING         -5
#define PSCI_INTERNAL_FAILURE   -6
#define PSCI_NOT_PRESENT        -7
#define PSCI_DISABLED           -8
#define PSCI_INVALID_ADDRESS    -9

u64 psci_call(u32 func, u64 cpuid, u64 entry, u64 ctxid);

#endif
