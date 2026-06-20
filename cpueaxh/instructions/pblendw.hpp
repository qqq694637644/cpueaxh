#pragma once

// instrusments/pblendw.hpp - PBLENDW implementation

#include "movdq.hpp"

static inline bool is_pblendw_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 4 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x3A && code[prefix_len + 2] == 0x0E;
}

static inline DecodedInstruction decode_pblendw_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_unsupported_simd_prefix = false;
    uint8_t mandatory_prefix = 0;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            if (mandatory_prefix == 0 || mandatory_prefix == prefix) {
                mandatory_prefix = prefix;
            }
            else {
                has_unsupported_simd_prefix = true;
            }
            if (prefix == 0x66) {
                ctx->operand_size_override = true;
            }
            offset++;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            offset++;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            ctx->rex_present = true;
            ctx->rex_w = ((prefix >> 3) & 1) != 0;
            ctx->rex_r = ((prefix >> 2) & 1) != 0;
            ctx->rex_x = ((prefix >> 1) & 1) != 0;
            ctx->rex_b = (prefix & 1) != 0;
            offset++;
        }
        else if (prefix == 0xF0) {
            has_lock_prefix = true;
            offset++;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65) {
            offset++;
        }
        else {
            break;
        }
    }

    if (has_lock_prefix || has_unsupported_simd_prefix || mandatory_prefix != 0x66) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (offset + 5 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset++] != 0x0F || code[offset++] != 0x3A || code[offset++] != 0x0E) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_movdq(ctx, &inst, code, code_size, &offset, false);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    inst.opcode = 0x0E;
    inst.imm_size = 1;
    inst.immediate = code[offset++];
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_pblendw(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    const DecodedInstruction inst = decode_pblendw_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
    const uint8_t mod = (inst.modrm >> 6) & 0x03;
    const XMMRegister source = (mod == 0x03)
        ? get_xmm128(ctx, decode_movdq_xmm_rm_index(ctx, inst.modrm))
        : read_xmm_memory(ctx, inst.mem_address);
    if (cpu_has_exception(ctx)) {
        return;
    }

    set_xmm128(ctx, dest, apply_avx2_blendw128(get_xmm128(ctx, dest), source, (uint8_t)inst.immediate));
}
