#include "aarch64.h"
#include "types.h"
#include "vcpu.h"
#include "printf.h"

#define ADDR_WBACK    (1 << 0)
#define ADDR_POSTIDX  (1 << 1)

enum addressing {
  OFFSET = 0,
  PRE_INDEX = ADDR_WBACK,
  POST_INDEX = ADDR_WBACK | ADDR_POSTIDX,
};

#define addressing_wback(ad)    ((ad) & ADDR_WBACK)
#define addressing_postidx(ad)  ((ad) & ADDR_POSTIDX)

struct arm_inst {
  ;
};

static int emul_ldnp_stnp(struct vcpu *vcpu, u32 inst) {
  return -1;
}

/* stp/ldp <Xt1>, <Xt2>, [<Xn>] */
static int emul_ldp_stp64(struct vcpu *vcpu, u32 inst, enum addressing ad, int l) {
  int rt = inst & 0x1f;
  int rn = (inst>>5) & 0x1f;
  int rt2 = (inst>>10) & 0x1f;
  int imm7 = (inst>>15) & 0x7f;

  printf("stp/ldp 64 rt %d rt2 %d rn %d imm7 %d\n", rt, rt2, rn, imm7);

  u64 addr = vcpu->reg.x[rn];
  if(!addressing_postidx(ad))
    addr += imm7;

  if(addressing_wback(ad))
    vcpu->reg.x[rn] = addr;

  return -1;
}

static int emul_ldp_stp(struct vcpu *vcpu, u32 inst, enum addressing ad) {
  int opc = (inst>>30) & 0x3;
  int v = (inst>>26) & 0x1;
  int l = (inst>>22) & 0x1;

  switch(opc) {
    case 2:
      if(v)
        panic("stp simd&fp");
      else
        return emul_ldp_stp64(vcpu, inst, ad, l);
    default: panic("emul_ldp_stp: unimplemeted");
  }
}

static int emul_load_store(struct vcpu *vcpu, u32 inst) {
#define ls_op0(inst)      (((inst)>>28) & 0x0f)
#define ls_op1(inst)      (((inst)>>26) & 0x01)
#define ls_op2(inst)      (((inst)>>23) & 0x03)
#define ls_op3(inst)      (((inst)>>16) & 0x3f)
#define ls_op4(inst)      (((inst)>>10) & 0x03)

  int op0 = ls_op0(inst);
  int op1 = ls_op1(inst);
  int op2 = ls_op2(inst);
  int op3 = ls_op3(inst);
  int op4 = ls_op4(inst);

  switch(op0 & 0x3) {
    case 2:   /* load and store pair */
      switch(op2) {
        case 0:
          return emul_ldnp_stnp(vcpu, inst);
        case 1:
          return emul_ldp_stp(vcpu, inst, POST_INDEX);
        case 2:
          return emul_ldp_stp(vcpu, inst, OFFSET);
        case 3:
          return emul_ldp_stp(vcpu, inst, PRE_INDEX);
      }
    default: panic("unimplemented");
  }
}

int cpu_emulate(struct vcpu *vcpu, u32 ai) {
  printf("cpu emulate %p %p\n", ai, vcpu->reg.elr);

  /* main encoding */
#define main_op0(inst)    (((inst)>>31) & 0x1)
#define main_op1(inst)    (((inst)>>25) & 0xf)

  int op0 = main_op0(ai);
  int op1 = main_op1(ai);

  switch(op1) {
    case 0: panic("unimplemented");
    case 1: panic("unallocated");
    case 2: panic("unimplemented: sve");
    case 3: panic("unallocated");
    default:
      switch((op1 >> 2) & 1) {
        case 0:
          panic("??");
        case 1:
          switch(op1 & 1) {
            case 0:
              return emul_load_store(vcpu, ai);
            case 1:
              panic("unimplemented");
          }
      }
  }

  return -1;
}
