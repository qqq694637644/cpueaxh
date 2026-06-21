#pragma once

// instructions/rdseed.hpp - RDSEED instruction implementation.

#include "rdrand.hpp"

static inline bool cpu_has_rdseed_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 18)) != 0;
}

inline bool is_rdseed_instruction(const uint8_t* code, size_t code_size) {
    size_t offset = 0;
    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0x67 || prefix == 0xF0 ||
            (prefix >= 0x40 && prefix <= 0x4F) ||
            prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
            ++offset;
        }
        else {
            break;
        }
    }
    if (offset + 3 > code_size || code[offset] != 0x0F || code[offset + 1] != 0xC7) {
        return false;
    }
    const uint8_t modrm = code[offset + 2];
    return (((modrm >> 3) & 0x07) == 7) && (((modrm >> 6) & 0x03) == 3);
}

static DecodedInstruction decode_rdseed_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;

    cpu_reset_prefix_state(ctx);
    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66) {
            ctx->operand_size_override = true;
            ++offset;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            ++offset;
        }
        else if (cpu_try_apply_rex_prefix(ctx, prefix)) {
            ++offset;
        }
        else if (prefix == 0xF0) {
            has_lock_prefix = true;
            ++offset;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
            ++offset;
        }
        else {
            break;
        }
    }

    if (has_lock_prefix || !cpu_has_rdseed_feature()) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (offset + 3 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
        return inst;
    }
    inst.opcode = code[offset++];
    if (inst.opcode != 0xC7) {
        raise_ud_ctx(ctx);
        return inst;
    }
    inst.has_modrm = true;
    inst.modrm = code[offset++];
    if (((inst.modrm >> 3) & 0x07) != 7 || ((inst.modrm >> 6) & 0x03) != 3) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }
    else {
        inst.operand_size = 32;
    }

    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_rdseed_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const int rm = decode_rdrand_rm_index(ctx, inst.modrm);

    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, false);
    set_flag(ctx, RFLAGS_ZF, false);
    set_flag(ctx, RFLAGS_AF, false);
    set_flag(ctx, RFLAGS_PF, false);

    if (inst.operand_size == 64) {
        unsigned __int64 value = 0;
        const unsigned char ok = _rdseed64_step(&value);
        set_reg64(ctx, rm, ok ? (uint64_t)value : 0ULL);
        set_flag(ctx, RFLAGS_CF, ok != 0);
        return;
    }
    if (inst.operand_size == 32) {
        unsigned int value = 0;
        const unsigned char ok = _rdseed32_step(&value);
        set_reg32(ctx, rm, ok ? (uint32_t)value : 0U);
        set_flag(ctx, RFLAGS_CF, ok != 0);
        return;
    }
    unsigned short value = 0;
    const unsigned char ok = _rdseed16_step(&value);
    set_reg16(ctx, rm, ok ? (uint16_t)value : 0U);
    set_flag(ctx, RFLAGS_CF, ok != 0);
}

void execute_rdseed(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_rdseed_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_rdseed_with_decoded(ctx, &inst);
}

inline void execute_rdseed_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_rdseed_with_decoded(ctx, &dec->cached);
}
