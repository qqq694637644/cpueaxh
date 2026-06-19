#pragma once

// instrusments/jcc.hpp - Jcc (Jump if Condition Is Met) instruction implementation

// --- Condition evaluation helpers ---

// Extract individual flags from RFLAGS
bool jcc_cf(CPU_CONTEXT* ctx) { return (ctx->rflags & RFLAGS_CF) != 0; }
bool jcc_zf(CPU_CONTEXT* ctx) { return (ctx->rflags & RFLAGS_ZF) != 0; }
bool jcc_sf(CPU_CONTEXT* ctx) { return (ctx->rflags & RFLAGS_SF) != 0; }
bool jcc_of(CPU_CONTEXT* ctx) { return (ctx->rflags & RFLAGS_OF) != 0; }
bool jcc_pf(CPU_CONTEXT* ctx) { return (ctx->rflags & RFLAGS_PF) != 0; }

// Evaluate the condition encoded in the low 4 bits of the Jcc opcode.
// Condition codes 0x0-0xF correspond to the standard Intel condition encoding:
//   0x0 JO  OF=1         0x1 JNO OF=0
//   0x2 JB  CF=1         0x3 JNB CF=0
//   0x4 JE  ZF=1         0x5 JNE ZF=0
//   0x6 JBE CF=1|ZF=1    0x7 JA  CF=0&ZF=0
//   0x8 JS  SF=1         0x9 JNS SF=0
//   0xA JP  PF=1         0xB JNP PF=0
//   0xC JL  SF!=OF       0xD JGE SF=OF
//   0xE JLE ZF=1|SF!=OF  0xF JG  ZF=0&SF=OF
bool eval_condition(CPU_CONTEXT* ctx, uint8_t cond) {
    bool cf = jcc_cf(ctx);
    bool zf = jcc_zf(ctx);
    bool sf = jcc_sf(ctx);
    bool of = jcc_of(ctx);
    bool pf = jcc_pf(ctx);

    switch (cond & 0x0F) {
    case 0x0: return of;                        // JO
    case 0x1: return !of;                       // JNO
    case 0x2: return cf;                        // JB / JC / JNAE
    case 0x3: return !cf;                       // JNB / JNC / JAE
    case 0x4: return zf;                        // JE / JZ
    case 0x5: return !zf;                       // JNE / JNZ
    case 0x6: return cf || zf;                  // JBE / JNA
    case 0x7: return !cf && !zf;               // JA / JNBE
    case 0x8: return sf;                        // JS
    case 0x9: return !sf;                       // JNS
    case 0xA: return pf;                        // JP / JPE
    case 0xB: return !pf;                       // JNP / JPO
    case 0xC: return sf != of;                  // JL / JNGE
    case 0xD: return sf == of;                  // JGE / JNL
    case 0xE: return zf || (sf != of);          // JLE / JNG
    case 0xF: return !zf && (sf == of);         // JG / JNLE
    default:  return false;
    }
}

// --- JCC execution helpers ---

// Perform the near branch: sign-extend displacement, add to RIP, mask for operand size
bool jcc_branch(CPU_CONTEXT* ctx, uint64_t base_rip, int64_t disp, int operand_size) {
    return cpu_assign_rip_checked(ctx, base_rip + (uint64_t)disp, operand_size);
}

// Short Jcc rel8 (opcodes 0x70-0x7F): sign-extend 8-bit displacement
void execute_jcc_short(CPU_CONTEXT* ctx, uint8_t cond, int8_t rel8, uint64_t next_rip, int operand_size) {
    if (eval_condition(ctx, cond)) {
        jcc_branch(ctx, next_rip, (int64_t)rel8, operand_size);
    }
    else {
        cpu_assign_rip_checked(ctx, next_rip, operand_size);
    }
}

// Near Jcc rel16/rel32 (opcodes 0x0F 0x80-0x8F): sign-extend 16/32-bit displacement
void execute_jcc_near(CPU_CONTEXT* ctx, uint8_t cond, int32_t rel, uint64_t next_rip, int operand_size) {
    if (eval_condition(ctx, cond)) {
        jcc_branch(ctx, next_rip, (int64_t)rel, operand_size);
    }
    else {
        cpu_assign_rip_checked(ctx, next_rip, operand_size);
    }
}

// JRCXZ/JECXZ/JCXZ (opcode 0xE3): test CX/ECX/RCX == 0 based on address size
void execute_jcxz(CPU_CONTEXT* ctx, int8_t rel8, uint64_t next_rip, int addr_size, int operand_size) {
    bool is_zero;
    if (addr_size == 64) {
        is_zero = (ctx->regs[REG_RCX] == 0);
    }
    else if (addr_size == 32) {
        is_zero = ((ctx->regs[REG_RCX] & 0xFFFFFFFF) == 0);
    }
    else {
        is_zero = ((ctx->regs[REG_RCX] & 0xFFFF) == 0);
    }

    if (is_zero) {
        jcc_branch(ctx, next_rip, (int64_t)rel8, operand_size);
    }
    else {
        cpu_assign_rip_checked(ctx, next_rip, operand_size);
    }
}

uint64_t decrement_loop_count(CPU_CONTEXT* ctx, int addr_size) {
    if (addr_size == 64) {
        ctx->regs[REG_RCX]--;
        return ctx->regs[REG_RCX];
    }
    if (addr_size == 32) {
        uint32_t ecx = (uint32_t)ctx->regs[REG_RCX];
        ecx--;
        ctx->regs[REG_RCX] = (ctx->regs[REG_RCX] & ~0xFFFFFFFFULL) | ecx;
        return ecx;
    }

    uint16_t cx = (uint16_t)ctx->regs[REG_RCX];
    cx--;
    ctx->regs[REG_RCX] = (ctx->regs[REG_RCX] & ~0xFFFFULL) | cx;
    return cx;
}

void execute_loopcc(CPU_CONTEXT* ctx, uint8_t opcode, int8_t rel8, uint64_t next_rip, int addr_size, int operand_size) {
    const uint64_t count = decrement_loop_count(ctx, addr_size);
    bool branch = count != 0;
    if (opcode == 0xE0) {
        branch = branch && ((ctx->rflags & RFLAGS_ZF) == 0);
    }
    else if (opcode == 0xE1) {
        branch = branch && ((ctx->rflags & RFLAGS_ZF) != 0);
    }

    if (branch) {
        jcc_branch(ctx, next_rip, (int64_t)rel8, operand_size);
    }
    else {
        cpu_assign_rip_checked(ctx, next_rip, operand_size);
    }
}

// --- Jcc instruction decoder + executor ---

void execute_jcc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    size_t offset = 0;

    bool rex_w = false;
    bool operand_size_override = false;
    bool address_size_override = false;

    // Decode prefixes
    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66) {
            operand_size_override = true;
            offset++;
        }
        else if (prefix == 0x67) {
            address_size_override = true;
            offset++;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            // REX prefix: only REX.W is relevant for Jcc
            rex_w = (prefix >> 3) & 1;
            ctx->rex_present = true;
            ctx->rex_w = rex_w;
            ctx->rex_r = (prefix >> 2) & 1;
            ctx->rex_x = (prefix >> 1) & 1;
            ctx->rex_b = prefix & 1;
            offset++;
        }
        else if (prefix == 0xF0) {
            // LOCK prefix is #UD for Jcc
            raise_ud_ctx(ctx);
            offset++;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
            offset++;
        }
        else {
            break;
        }
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return;
    }
    if (cpu_has_exception(ctx)) {
        return;
    }

    // In 64-bit mode: operand size fixed at 64; rel8/rel32 sign-extended to 64 bits
    // In 32-bit mode: default 32, 0x66 overrides to 16
    int operand_size;
    if (ctx->cs.descriptor.long_mode) {
        // 64-bit mode: size always 64 for branches
        operand_size = 64;
    }
    else {
        operand_size = 32;
        if (rex_w) {
            operand_size = 64;
        }
        else if (operand_size_override) {
            operand_size = 16;
        }
    }

    // Address size determines which register JRCXZ/JECXZ/JCXZ tests
    int addr_size;
    if (ctx->cs.descriptor.long_mode) {
        addr_size = address_size_override ? 32 : 64;
    }
    else {
        addr_size = address_size_override ? 16 : 32;
    }

    uint8_t opcode = code[offset++];

    if (opcode >= 0xE0 && opcode <= 0xE3) {
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }
        int8_t rel8 = (int8_t)code[offset++];
        ctx->last_inst_size = (int)offset;
        uint64_t next_rip = ctx->rip + (uint64_t)offset;
        if (opcode == 0xE3) {
            // JRCXZ / JECXZ / JCXZ - test CX/ECX/RCX == 0
            execute_jcxz(ctx, rel8, next_rip, addr_size, operand_size);
        }
        else {
            execute_loopcc(ctx, opcode, rel8, next_rip, addr_size, operand_size);
        }
        return;
    }

    if (opcode >= 0x70 && opcode <= 0x7F) {
        // Short Jcc: rel8
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }
        int8_t rel8 = (int8_t)code[offset++];
        uint8_t cond = opcode & 0x0F;
        ctx->last_inst_size = (int)offset;
        uint64_t next_rip = ctx->rip + (uint64_t)offset;
        execute_jcc_short(ctx, cond, rel8, next_rip, operand_size);
        return;
    }

    if (opcode == 0x0F) {
        // Near Jcc: 0x0F 0x80-0x8F
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }
        uint8_t opcode2 = code[offset++];
        if (opcode2 < 0x80 || opcode2 > 0x8F) {
            raise_ud_ctx(ctx);
        }
        uint8_t cond = opcode2 & 0x0F;

        // In 64-bit mode rel16 is N.S. (not supported); rel32 is used
        int imm_size;
        if (ctx->cs.descriptor.long_mode) {
            // rel16 not supported in 64-bit mode; always rel32
            if (operand_size_override) {
                raise_ud_ctx(ctx);
            }
            imm_size = 4;
        }
        else {
            imm_size = operand_size_override ? 2 : 4;
        }

        if (offset + imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }

        int32_t rel = 0;
        for (int i = 0; i < imm_size; i++) {
            rel |= ((int32_t)code[offset++]) << (i * 8);
        }
        // Sign-extend 16-bit immediate if needed
        if (imm_size == 2) {
            rel = (int16_t)rel;
        }

        ctx->last_inst_size = (int)offset;
        uint64_t next_rip = ctx->rip + (uint64_t)offset;
        execute_jcc_near(ctx, cond, rel, next_rip, operand_size);
        return;
    }

    raise_ud_ctx(ctx);
}
