/*
 *  armv8 instruction emulation
 */

#include "aarch64.h"
#include "types.h"
#include "vcpu.h"
#include "vsm.h"
#include "mm.h"
#include "log.h"
#include "lib.h"
#include "node.h"
#include "emul.h"

#define ADDR_WBACK    (1 << 0)
#define ADDR_POSTIDX  (1 << 1)

enum addressing {
  OFFSET = 0,
  PRE_INDEX = ADDR_WBACK,
  POST_INDEX = ADDR_WBACK | ADDR_POSTIDX,
};

enum regext {
  EXT_UXTB = 0,
  EXT_UXTH = 1,
  EXT_UXTW = 2,
  EXT_UXTX = 3,
  EXT_SXTB = 4,
  EXT_SXTH = 5,
  EXT_SXTW = 6, 
  EXT_SXTX = 7,
};

#define addressing_wback(ad)    ((ad) & ADDR_WBACK)
#define addressing_postidx(ad)  ((ad) & ADDR_POSTIDX)

static int emul_ldnp_stnp(struct vcpu *vcpu, u32 inst) {
  panic("ldnp stnp");

  return -1;
}

/* ldp/stp <Xt1>, <Xt2>, [<Xn>] */
static int emul_ldpstp(struct vcpu *vcpu, u32 inst, enum addressing ad, int opc, int load) {
  int rt = inst & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int rt2 = (inst >> 10) & 0x1f;
  int imm7 = (int)(inst << (32 - 15 - 7)) >> (32 - 7);
  int scale = 2 + get_bit(opc, 1);
  int offset = imm7 << scale;
  int datasize = 8 << scale;  /* 32 or 64 */
  u64 addr;

  if(rn == 31)
    addr = vcpu->reg.sp;
  else
    addr = vcpu->reg.x[rn];

  u64 ipa = vcpu->dabt.fault_ipa;

  if(!addressing_postidx(ad))   /* pre-index */
    addr += offset;

  if(datasize == 32) {
    u32 rtval = rt == 31 ? 0 : (u32)vcpu->reg.x[rt];
    u32 rt2val = rt2 == 31 ? 0 : (u32)vcpu->reg.x[rt2];

    u32 reg[2] = { rtval, rt2val };

    if(load) {   /* ldp */
      if(vsm_access(vcpu, (char *)reg, ipa, 8, 0) < 0)
        return -1;
      vcpu->reg.x[rt] = reg[0];
      vcpu->reg.x[rt2] = reg[1];
    } else {    /* stp */
      if(vsm_access(vcpu, (char *)reg, ipa, 8, 1) < 0)
        return -1;
    }
  } else if(datasize == 64) {
    u64 rtval = rt == 31 ? 0 : vcpu->reg.x[rt];
    u64 rt2val = rt2 == 31 ? 0 : vcpu->reg.x[rt2];

    u64 reg[2] = { rtval, rt2val };

    if(load) {   /* ldp */
      if(vsm_access(vcpu, (char *)reg, ipa, 16, 0) < 0)
        return -1;
      vcpu->reg.x[rt] = reg[0];
      vcpu->reg.x[rt2] = reg[1];
    } else {    /* stp */
      if(vsm_access(vcpu, (char *)reg, ipa, 16, 1) < 0)
        return -1;
    }
  } else {
    panic("unreachable");
  }

  if(addressing_wback(ad)) {
    if(addressing_postidx(ad))
      addr += offset;
    if(rn == 31)
      vcpu->reg.sp = addr;
    else
      vcpu->reg.x[rn] = addr;
  }

  return 0;
}

static int emul_ldst_pair(struct vcpu *vcpu, u32 inst, enum addressing ad) {
  int opc = (inst >> 30) & 0x3;
  int v = (inst >> 26) & 0x1;
  int l = (inst >> 22) & 0x1;

  if(v)
    panic("stp simd&fp");

  switch(opc) {
    case 0: case 2:
      return emul_ldpstp(vcpu, inst, ad, opc, l);
    default: panic("emul_ldp_stp: unimplemeted");
  }
}

static int emul_ldxr(struct vcpu *vcpu, u32 inst, int size) {
  u64 ipa = vcpu->dabt.fault_ipa;
  u64 page = ipa & ~(u64)(PAGESIZE-1);

  vsm_read_fetch_page(page);

  return 1;
}

static int emul_stxr(struct vcpu *vcpu, u32 inst, int size) {
  u64 ipa = vcpu->dabt.fault_ipa;
  u64 page = ipa & ~(u64)(PAGESIZE-1);

  vmm_log("stxr!?fdklafajlfkjlaksfjdlskjflajfklajfklasjf");

  vsm_write_fetch_page(page);

  return 1;
}

static int emul_ldst_excl(struct vcpu *vcpu, u32 inst) {
  int size = (inst >> 30) & 0x3;
  int l = (inst >> 22) & 0x1;
  int o0 = (inst >> 15) & 0x1;

  /* TODO: o0? */
  // if(o0)
  //   panic("o0");

  if(l)
    return emul_ldxr(vcpu, inst, size);
  else
    return emul_stxr(vcpu, inst, size);
}

static int emul_ldrstr_roffset(struct vcpu *vcpu, int rt, int size, bool load) {
  u64 ipa = vcpu->dabt.fault_ipa;
  int accbyte = 1 << size;
  int c;

  if(load) {
    u64 val = 0;
    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 0) < 0)
      return -1;
    vcpu->reg.x[rt] = val;
  } else {
    u64 val = rt == 31 ? 0 : vcpu->reg.x[rt];
    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 1) < 0)
      return -1;
  }

  return 0;
}

static int emul_ldst_roffset(struct vcpu *vcpu, u32 inst) {
  int rt = inst & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int s = (inst >> 12) & 0x1;
  int option = (inst >> 13) & 0x7;
  int rm = (inst >> 16) & 0x1f;
  int opc = (inst >> 22) & 0x3;
  int v = (inst >> 26) & 0x1;
  int size = (inst >> 30) & 0x3;
  bool load = opc != 0;

  if(v)
    panic("vector");

  return emul_ldrstr_roffset(vcpu, rt, size, load);
}

/* emulate ldr* str* */
static int emul_ldrstr_imm(struct vcpu *vcpu, int rt, int rn, int imm,
                           int size, bool load, enum addressing ad) {
  u64 addr;
  u64 ipa = vcpu->dabt.fault_ipa;
  int accbyte = 1 << size;

  if(rn == 31)
    addr = vcpu->reg.sp;
  else
    addr = vcpu->reg.x[rn];

  if(!addressing_postidx(ad))   /* pre-index */
    addr += imm;

  if(load) {
    u64 val = 0;
    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 0) < 0)
      return -1;
    vcpu->reg.x[rt] = val;
  } else {
    u64 val = rt == 31 ? 0 : vcpu->reg.x[rt];

    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 1) < 0)
      return -1;
  }

  if(addressing_wback(ad)) {      /* writeback */
    if(addressing_postidx(ad))    /* post-index */
      addr += imm;
    if(rn == 31)
      vcpu->reg.sp = addr;
    else
      vcpu->reg.x[rn] = addr;
  }

  return 0;
}

/* load/store register imm9
 *  - immediate post-indexed
 *  - immediate pre-indexed
 */
static int emul_ldst_reg_imm9(struct vcpu *vcpu, u32 inst, enum addressing ad) {
  int rt = inst & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int imm9 = (int)(inst << (32 - 12 - 9)) >> (32 - 9);
  int opc = (inst >> 22) & 0x3; 
  int v = (inst >> 26) & 0x1;
  int size = (inst >> 30) & 0x3;
  bool load = (opc & 1) != 0;

  if(v)
    panic("vector unsupported");

  return emul_ldrstr_imm(vcpu, rt, rn, imm9, size, load, ad);
}

static int emul_ldst_reg_uimm(struct vcpu *vcpu, u32 inst) {
  int rt = inst & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int imm12 = (inst >> 10) & 0xfff;
  int opc = (inst >> 22) & 0x3; 
  int v = (inst >> 26) & 0x1;
  int size = (inst >> 30) & 0x3;

  if(v)
    panic("vector unsupported");

  bool load = opc != 0;

  return emul_ldrstr_imm(vcpu, rt, rn, imm12, size, load, OFFSET);
}

static int emul_ldst_reg_unscaled(struct vcpu *vcpu) {
  u64 ipa = vcpu->dabt.fault_ipa;
  if(!vcpu->dabt.isv)
    panic("isv");
  bool wr = vcpu->dabt.write;
  int reg = vcpu->dabt.reg;
  int accbyte = vcpu->dabt.accbyte;

  if(wr) {
    u64 val = reg == 31 ? 0 : vcpu->reg.x[reg];

    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 1) < 0)
      return -1;
  } else {
    u64 val = 0;
    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 0) < 0)
      return -1;
    vcpu->reg.x[reg] = val;
  }

  return 0;
}

static int emul_ldar_stlr(struct vcpu *vcpu, int size, int rn, int rt, bool load) {
  u64 ipa = vcpu->dabt.fault_ipa;
  int accbyte = 1 << size;

  if(load) {
    u64 val = 0;
    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 0) < 0)
      return -1;
    vcpu->reg.x[rt] = val;
  } else {
    u64 val = rt == 31 ? 0 : vcpu->reg.x[rt];
    if(vsm_access(vcpu, (char *)&val, ipa, accbyte, 1) < 0)
      return -1;
  }

  return 0;
}

static int emul_ldst_ordered(struct vcpu *vcpu, u32 inst) {
  int size = (inst >> 30) & 0x3;
  int l = (inst >> 22) & 0x1;
  int rs = (inst >> 16) & 0x1f;
  int o0 = (inst >> 15) & 0x1;
  int rt2 = (inst >> 10) & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int rt = inst & 0x1f;

  if(!o0)
    panic("!o0");

  return emul_ldar_stlr(vcpu, size, rn, rt, l);
}

static int emul_load_store(struct vcpu *vcpu, u32 inst) {
  int op0 = (inst >> 28) & 0x0f;
  int op1 = (inst >> 26) & 0x01;
  int op2 = (inst >> 23) & 0x03;
  int op3 = (inst >> 16) & 0x3f;
  int op4 = (inst >> 10) & 0x03;

  switch(op0 & 0x3) {
    case 0:
      switch(op1) {
        case 0:
          switch(op2) {
            case 0:
              return emul_ldst_excl(vcpu, inst);
            case 1:
              switch((op3 >> 5) & 0x1) {
                case 0: return emul_ldst_ordered(vcpu, inst);
                case 1: goto unimpl;
              }
            case 2: case 3:
              goto unimpl;
          }
        case 1: goto unimpl;
      }
    case 1: goto unimpl;
    case 2:   /* load and store pair */
      switch(op2) {
        case 0: return emul_ldnp_stnp(vcpu, inst);
        case 1: return emul_ldst_pair(vcpu, inst, POST_INDEX);
        case 2: return emul_ldst_pair(vcpu, inst, OFFSET);
        case 3: return emul_ldst_pair(vcpu, inst, PRE_INDEX);
      }
    case 3:
      switch((op2 >> 1) & 0x1) {
        case 0:
          switch((op3 >> 5) & 0x1) {
            case 0:
              switch(op4) {
                case 0: return emul_ldst_reg_unscaled(vcpu);
                case 1: return emul_ldst_reg_imm9(vcpu, inst, POST_INDEX);
                case 2: panic("ldst unprivileged");
                case 3: return emul_ldst_reg_imm9(vcpu, inst, PRE_INDEX);
              }
            case 1:
              switch(op4) {
                case 0:
                  panic("atomic");
                case 2:
                  return emul_ldst_roffset(vcpu, inst);
                case 1: case 3:
                  panic("emul_ldst_pac");
              }
          }
        case 1:
          return emul_ldst_reg_uimm(vcpu, inst);
      }
    default:  return -1;
  }

unimpl:
  vmm_warn("cannot emulate %p %p\n", inst, vcpu->reg.elr);
  return -1;
}

int cpu_emulate(struct vcpu *vcpu, u32 inst) {
  /* main encoding */
  int op0 = (inst >> 31) & 0x1;
  int op1 = (inst >> 25) & 0xf;

  switch(op1) {
    case 0x0:
      goto err;
    case 0x1:
      panic("unallocated");
    case 0x2:
      goto err;
    case 0x3:
      panic("unallocated");
    case 0x8: case 0x9:
      panic("data ?");
    case 0xa: case 0xb:
      panic("branch ?");
    case 0x4: case 0x6: case 0xc: case 0xe:   /* load and stores */
      return emul_load_store(vcpu, inst);
    case 0x5: case 0xd:
    case 0x7: case 0xf:
      panic("data ?");
  }

err:
  vmm_warn("cannot emulate %p %p\n", inst, vcpu->reg.elr);
  return -1;
}
