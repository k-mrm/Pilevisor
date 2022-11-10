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
#include "panic.h"

#define ADDR_WBACK    (1 << 0)
#define ADDR_POSTIDX  (1 << 1)

#define X(vcpu, rt)   ((rt) == 31 ? 0 : (vcpu)->reg.x[(rt)])

enum addressing {
  OFFSET = 0,
  PRE_INDEX = ADDR_WBACK,
  POST_INDEX = ADDR_WBACK | ADDR_POSTIDX,
};

#define addressing_wback(ad)    ((ad) & ADDR_WBACK)
#define addressing_postidx(ad)  ((ad) & ADDR_POSTIDX)

/*
 *  @sext32 used for ldrsh/ldrsw
 */
static int load_register(struct vcpu *vcpu, int rt, u64 ipa, int size, bool sign_extend, bool sext32) {
  switch(size) {
    case 0: {
      u8 val = 0;
      if(vsm_access(vcpu, (char *)&val, ipa, 1, 0) < 0)
        return -1;

      if(sign_extend) {
        if(sext32)
          vcpu->reg.x[rt] = (i32)(i8)val;
        else
          vcpu->reg.x[rt] = (i64)(i8)val;
      } else {
        vcpu->reg.x[rt] = val;
      }
      return 0;
    }
    case 1: {
      u16 val = 0;
      if(vsm_access(vcpu, (char *)&val, ipa, 2, 0) < 0)
        return -1;

      if(sign_extend) {
        if(sext32)
          vcpu->reg.x[rt] = (i32)(i16)val;
        else
          vcpu->reg.x[rt] = (i64)(i16)val;
      } else {
        vcpu->reg.x[rt] = val;
      }
      return 0;
    }
    case 2: {
      u32 val = 0;
      if(vsm_access(vcpu, (char *)&val, ipa, 4, 0) < 0)
        return -1;
      vcpu->reg.x[rt] = sign_extend ? (i64)(i32)val : val;
      return 0;
    }
    case 3: {
      u64 val = 0;
      if(vsm_access(vcpu, (char *)&val, ipa, 8, 0) < 0)
        return -1;
      vcpu->reg.x[rt] = val;
      return 0;
    }
    default:
      panic("unreachable");
  }
}

static int store_register(struct vcpu *vcpu, int rt, u64 ipa, int size) {
  switch(size) {
    case 0: {
      u8 val = rt == 31 ? 0 : (u8)vcpu->reg.x[rt];
      if(vsm_access(vcpu, (char *)&val, ipa, 1, 1) < 0)
        return -1;
      return 0;
    }
    case 1: {
      u16 val = rt == 31 ? 0 : (u16)vcpu->reg.x[rt];
      if(vsm_access(vcpu, (char *)&val, ipa, 2, 1) < 0)
        return -1;
      return 0;
    }
    case 2: {
      u32 val = rt == 31 ? 0 : (u32)vcpu->reg.x[rt];
      if(vsm_access(vcpu, (char *)&val, ipa, 4, 1) < 0)
        return -1;
      return 0;
    }
    case 3: {
      u64 val = rt == 31 ? 0UL : vcpu->reg.x[rt];
      if(vsm_access(vcpu, (char *)&val, ipa, 8, 1) < 0)
        return -1;
      return 0;
    }
    default:
      panic("unreachable");
  }
}

/* ldp/stp <Xt1>, <Xt2>, [<Xn>] */
static int emul_ldpstp(struct vcpu *vcpu, int rn, int rt, int rt2, int imm7,
                       int opc, enum addressing ad, bool load) {
  /*
  int scale = 2 + ((opc >> 1) & 1);
  int offset = imm7 << scale;
  int datasize = 8 << scale;
  u64 addr;

  if(rn == 31)
    addr = vcpu->reg.sp;
  else
    addr = vcpu->reg.x[rn];

  u64 ipa = vcpu->dabt.fault_ipa;

  if(!addressing_postidx(ad))
    addr += offset;

  if((addr & 0xfff) != (ipa & 0xfff))
    panic("emul: bug %p %p %d %d %p\n", addr, ipa, offset, rn, read_sysreg(far_el2));

  if(datasize == 32) {
    ;
  } else if(datasize == 64) {
    ;
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
  */

  u64 page = PAGE_ADDRESS(vcpu->dabt.fault_ipa);

  if(load)
    vsm_read_fetch_page(page);
  else
    vsm_write_fetch_page(page);

  return 1;
}

static int emul_ldst_pair(struct vcpu *vcpu, u32 inst, enum addressing ad) {
  int rt = inst & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int rt2 = (inst >> 10) & 0x1f;
  int imm7 = (int)(inst << (32 - 15 - 7)) >> (32 - 7);
  int l = (inst >> 22) & 0x1;
  int v = (inst >> 26) & 0x1;
  int opc = (inst >> 30) & 0x3;

  if(v)
    panic("ldp/stp simd&fp %p", inst);

  if(opc & 1)
    panic("unimplemented stgp/ldpsw");

  return emul_ldpstp(vcpu, rn, rt, rt2, imm7, opc, ad, l);
}

static int emul_ldxr(struct vcpu *vcpu) {
  u64 page = PAGE_ADDRESS(vcpu->dabt.fault_ipa);

  vsm_read_fetch_page(page);

  return 1;
}

static int emul_stxr(struct vcpu *vcpu) {
  u64 page = PAGE_ADDRESS(vcpu->dabt.fault_ipa);

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
    return emul_ldxr(vcpu);
  else
    return emul_stxr(vcpu);
}

static int emul_ldr_roffset(struct vcpu *vcpu, int rt, int size, bool sign_extend, bool sext32) {
  u64 ipa = vcpu->dabt.fault_ipa;

  if(load_register(vcpu, rt, ipa, size, sign_extend, sext32) < 0)
    return -1;

  return 0;
}

static int emul_str_roffset(struct vcpu *vcpu, int rt, int size) {
  u64 ipa = vcpu->dabt.fault_ipa;

  if(store_register(vcpu, rt, ipa, size) < 0)
    return -1;

  return 0;
}

static int emul_ldr_imm(struct vcpu *vcpu, int rt, int rn, int imm, int size, enum addressing ad, bool sign_extend, bool sext32) {
  u64 addr;
  u64 ipa = vcpu->dabt.fault_ipa;

  if(rn == 31)
    addr = vcpu->reg.sp;
  else
    addr = vcpu->reg.x[rn];

  if(!addressing_postidx(ad))   /* pre-index */
    addr += imm;

  if((addr & 0xfff) != (ipa & 0xfff))
    panic("emul: bug %p %p\n", addr, ipa);

  if(load_register(vcpu, rt, ipa, size, sign_extend, sext32) < 0)
    return -1;

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

static int emul_str_imm(struct vcpu *vcpu, int rt, int rn, int imm, int size, enum addressing ad) {
  u64 addr;
  u64 ipa = vcpu->dabt.fault_ipa;

  if(rn == 31)
    addr = vcpu->reg.sp;
  else
    addr = vcpu->reg.x[rn];

  if(!addressing_postidx(ad))   /* pre-index */
    addr += imm;

  if((addr & 0xfff) != (ipa & 0xfff))
    panic("emul: bug %p %p %d %d\n", addr, ipa, rn, imm);

  if(store_register(vcpu, rt, ipa, size) < 0)
    return -1;

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
  bool sign_extend = (opc >> 1) & 0x1;
  bool opc_0 = opc & 0x1;

  if(v)
    panic("vector");

  if(load)
    return emul_ldr_roffset(vcpu, rt, size, sign_extend, opc_0);
  else
    return emul_str_roffset(vcpu, rt, size);
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
  bool sign_extend = (opc >> 1) & 0x1;
  bool opc_0 = opc & 0x1;

  if(v)
    panic("vector unsupported");

  if(load)
    return emul_ldr_imm(vcpu, rt, rn, imm9, size, ad, sign_extend, opc_0);
  else
    return emul_str_imm(vcpu, rt, rn, imm9, size, ad);
}

static int emul_ldst_reg_uimm(struct vcpu *vcpu, u32 inst) {
  int rt = inst & 0x1f;
  int rn = (inst >> 5) & 0x1f;
  int imm12 = (inst >> 10) & 0xfff;
  int opc = (inst >> 22) & 0x3; 
  int v = (inst >> 26) & 0x1;
  int size = (inst >> 30) & 0x3;

  bool load = opc != 0;
  bool sign_extend = (opc >> 1) & 0x1;
  bool opc_0 = opc & 0x1;

  int offset = imm12 << size;

  if(v)
    panic("vector unsupported");

  if(size == 3 && opc == 2)
    panic("pfrm ()");

  if(load)
    return emul_ldr_imm(vcpu, rt, rn, offset, size, OFFSET, sign_extend, opc_0);
  else
    return emul_str_imm(vcpu, rt, rn, offset, size, OFFSET);
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
        case 0:
          // return emul_ldnp_stnp(vcpu, inst);
          return emul_ldst_pair(vcpu, inst, OFFSET);
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
