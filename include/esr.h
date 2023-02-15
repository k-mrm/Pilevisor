#ifndef AARCH64_ESR_EL2_H
#define AARCH64_ESR_EL2_H

#define FSC_PERM_FAULT      (3 << 2)

/* ISS encoding for an exception from a Data Abort */
#define ESR_DAbort_WnR      (1 << 6)
#define ESR_DAbort_S1PTW    (1 << 7)
#define ESR_DAbort_CM       (1 << 8)
#define ESR_DAbort_EA       (1 << 9)
#define ESR_DAbort_FnV      (1 << 10)

/* ISS encoding for an exception from a Instruction Abort */

#endif  /* AARCH64_ESR_EL2_H */
