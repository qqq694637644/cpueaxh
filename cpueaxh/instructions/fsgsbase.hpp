#pragma once

// instructions/fsgsbase.hpp - RDFSBASE/RDGSBASE instruction implementation.

#include "instruction_common.hpp"

static inline bool cpu_has_fsgsbase_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 0)) != 0;
}

static inline int decode_fsgsbase_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static DecodedInstruction decode_fsgsbase_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_f3_prefix = false;
    bool has_unsupported_simd_prefix = false;

    cpu_reset_prefix_state(ctx);

    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0xF3) {
            has_f3_prefix = true;
            ++offset;
        }
        else if (prefix == 0x66 || prefix == 0xF2) {
            has_unsupported_simd_prefix = true;
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
                 prefix == 0x64 || prefix == 0x65) {
            ++offset;
        }
        else {
            break;
        }
    }

    if (has_lock_prefix || has_unsupported_simd_prefix || !has_f3_prefix ||
        !ctx->cs.descriptor.long_mode || !cpu_has_fsgsbase_feature()) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (offset + 3 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    if (code[offset++] != 0x0F || code[offset++] != 0xAE) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.opcode = 0xAE;
    inst.has_modrm = true;
    inst.modrm = code[offset++];
    const uint8_t mod = (inst.modrm >> 6) & 0x03;
    const uint8_t reg = (inst.modrm >> 3) & 0x07;
    if (mod != 3 || reg > 1) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.operand_size = ctx->rex_w ? 64 : 32;
    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_fsgsbase_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const int dest = decode_fsgsbase_rm_index(ctx, inst.modrm);
    const uint8_t reg = (inst.modrm >> 3) & 0x07;
    const uint64_t base = (reg == 0) ? ctx->fs.descriptor.base : ctx->gs.descriptor.base;
    if (inst.operand_size == 64) {
        set_reg64(ctx, dest, base);
    }
    else {
        set_reg32(ctx, dest, (uint32_t)base);
    }
}

void execute_fsgsbase(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_fsgsbase_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_fsgsbase_with_decoded(ctx, &inst);
}

inline void execute_fsgsbase_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_fsgsbase_with_decoded(ctx, &dec->cached);
}
