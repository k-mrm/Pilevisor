#include "aarch64.h"
#include "types.h"
#include "vcpu.h"
#include "vsm.h"
#include "log.h"

#define ADDR_WBACK    (1 << 0)
#define ADDR_POSTIDX  (1 << 1)

enum addressing {
  OFFSET = 0,
  PRE_INDEX = ADDR_WBACK,
  POST_INDEX = ADDR_WBACK | ADDR_POSTIDX,
};

#define addressing_wback(ad)    ((ad) & ADDR_WBACK)
#define addressing_postidx(ad)  ((ad) & ADDR_POSTIDX)

static int emul_ldnp_stnp(struct vcpu *vcpu, u32 inst) {
  return -1;
}

/* stp/ldp <Xt1>, <Xt2>, [<Xn>] */
static int emul_ldp_stp64(struct vcpu *vcpu, u32 inst, enum addressing ad, int l) {
  int rt = inst & 0x1f;
  int rn = (inst>>5) & 0x1f;
  int rt2 = (inst>>10) & 0x1f;
  int imm7 = (inst>>15) & 0x7f;

  u64 addr = vcpu->reg.x[rn];
  if(!addressing_postidx(ad))   /* pre-index */
    addr += imm7 << 3;

  u64 reg[2] = { vcpu->reg.x[rt], vcpu->reg.x[rt2] };
  if(l) {   /* ldp */
    vsm_access(vcpu, (char *)reg, addr, 16, 0);
    vcpu->reg.x[rt] = reg[0];
    vcpu->reg.x[rt2] = reg[1];
  }
  else {    /* stp */
    vsm_access(vcpu, (char *)reg, addr, 16, 1);
  }

  if(addressing_wback(ad))
    vcpu->reg.x[rn] = addr;

  return 0;
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

/* access (1 << size) byte */
static int emul_ldxr(struct vcpu *vcpu, u32 inst, int size) {
  int rn = (inst>>5) & 0x1f;
  int rt = inst & 0x1f;
  u64 addr;

  if(rt == 31)
    panic("sp");
  else
    addr = vcpu->reg.x[rn];

  vsm_access(vcpu, (char *)&vcpu->reg.x[rt], addr, 1 << size, 0);

  return 0;
}

static int emul_stxr(struct vcpu *vcpu, u32 inst, int size) {
  int rs = (inst>>16) & 0x1f;
  int rt2 = (inst>>10) & 0x1f;
  int rn = (inst>>5) & 0x1f;
  int rt = inst & 0x1f;

  panic("?");

  return -1;
}

static int emul_ld_st_exculsive(struct vcpu *vcpu, u32 inst) {
  int size = (inst>>30) & 0x3;
  int l = (inst>>22) & 0x1;
  int o0 = (inst>>15) & 0x1;

  if(o0) panic("?");

  if(vcpu->reg.elr == 0xffff800010471eec) {
    printf("ccccc %d %d %d\n", size, l, o0);
  }

  if(l)
    return emul_ldxr(vcpu, inst, size);
  else
    return emul_stxr(vcpu, inst, size);
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
    case 0:
      switch(op1) {
        case 0:
          switch(op2) {
            case 0:  return emul_ld_st_exculsive(vcpu, inst);
            default: return -1;
          }
        case 1: return -1;
      }
    case 1: return -1;
    case 2:   /* load and store pair */
      switch(op2) {
        case 0: return emul_ldnp_stnp(vcpu, inst);
        case 1: return emul_ldp_stp(vcpu, inst, POST_INDEX);
        case 2: return emul_ldp_stp(vcpu, inst, OFFSET);
        case 3: return emul_ldp_stp(vcpu, inst, PRE_INDEX);
      }
    default: return -1;
  }
}

int cpu_emulate(struct vcpu *vcpu, u32 ai) {
  /* main encoding */
#define main_op0(inst)    (((inst)>>31) & 0x1)
#define main_op1(inst)    (((inst)>>25) & 0xf)

  int op0 = main_op0(ai);
  int op1 = main_op1(ai);

  switch(op1) {
    case 0: goto unimpl;
    case 1: panic("unallocated");
    case 2: goto unimpl;
    case 3: panic("unallocated");
    default:
      switch((op1 >> 2) & 1) {
        case 0:
          goto unimpl;
        case 1:
          switch(op1 & 1) {
            case 0:
              if(emul_load_store(vcpu, ai) < 0)
                goto unimpl;
              return 0;
            case 1:
              goto unimpl;
          }
      }
  }

unimpl:
  vmm_warn("cannot emulate %p %p\n", ai, vcpu->reg.elr);

  return -1;
}
