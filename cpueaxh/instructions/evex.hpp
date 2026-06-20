#pragma once

// instrusments/evex.hpp - Shared EVEX prefix/decode helpers

#include "movdq.hpp"

#define CPUEAXH_EVEX_VL_128 0x01
#define CPUEAXH_EVEX_VL_256 0x02
#define CPUEAXH_EVEX_VL_512 0x04

struct EVEXPrefix {
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t opcode;
};

struct EVEXDecodedInstructionInfo {
    EVEXPrefix prefix;
    int vector_bits;
    int source1;
    int writemask;
    bool zeroing;
};

static inline EVEXPrefix decode_evex_prefix(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    EVEXPrefix prefix = {};
    if (code_size < 7) {
        raise_gp_ctx(ctx, 0);
        return prefix;
    }
    if (code[0] != 0x62) {
        raise_ud_ctx(ctx);
        return prefix;
    }

    prefix.byte1 = code[1];
    prefix.byte2 = code[2];
    prefix.byte3 = code[3];
    prefix.opcode = code[4];
    return prefix;
}

static inline int evex_source1_index(const EVEXPrefix* prefix) {
    return (int)((~((prefix->byte2 >> 3) & 0x0F)) & 0x0F);
}

static inline int evex_writemask_index(const EVEXPrefix* prefix) {
    return (int)(prefix->byte3 & 0x07);
}

static inline bool evex_zeroing(const EVEXPrefix* prefix) {
    return (prefix->byte3 & 0x80) != 0;
}

static inline int evex_decode_vector_bits(CPU_CONTEXT* ctx, const EVEXPrefix* prefix, int allowed_vector_mask) {
    int vector_bits = 0;
    switch (prefix->byte3 & 0x60) {
    case 0x00:
        vector_bits = 128;
        break;
    case 0x20:
        vector_bits = 256;
        break;
    case 0x40:
        vector_bits = 512;
        break;
    default:
        raise_ud_ctx(ctx);
        return 0;
    }

    const int vector_flag = (vector_bits == 128) ? CPUEAXH_EVEX_VL_128
        : (vector_bits == 256) ? CPUEAXH_EVEX_VL_256
        : CPUEAXH_EVEX_VL_512;
    if ((allowed_vector_mask & vector_flag) == 0) {
        raise_ud_ctx(ctx);
        return 0;
    }
    return vector_bits;
}

static inline void apply_evex_state(CPU_CONTEXT* ctx, const EVEXPrefix* prefix) {
    ctx->rex_present = true;
    ctx->rex_w = ((prefix->byte2 >> 7) & 1) != 0;
    ctx->rex_r = (prefix->byte1 & 0x10) == 0;
    ctx->rex_x = (prefix->byte1 & 0x40) == 0;
    ctx->rex_b = (prefix->byte1 & 0x20) == 0;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;
}

static inline bool evex_validate_common(
    CPU_CONTEXT* ctx,
    const EVEXPrefix* prefix,
    uint8_t opcode,
    uint8_t map_select,
    uint8_t mandatory_prefix,
    int allowed_vector_mask,
    bool allow_writemask,
    bool allow_zeroing,
    EVEXDecodedInstructionInfo* info) {
    if (prefix->opcode != opcode ||
        (prefix->byte1 & 0x03) != map_select ||
        (prefix->byte2 & 0x03) != mandatory_prefix ||
        (prefix->byte2 & 0x04) == 0 ||
        (prefix->byte3 & 0x18) != 0x08) {
        raise_ud_ctx(ctx);
        return false;
    }

    if (info) {
        info->vector_bits = evex_decode_vector_bits(ctx, prefix, allowed_vector_mask);
        if (cpu_has_exception(ctx)) {
            return false;
        }
        info->source1 = evex_source1_index(prefix);
        info->writemask = evex_writemask_index(prefix);
        info->zeroing = evex_zeroing(prefix);
    }

    if (!allow_writemask && evex_writemask_index(prefix) != 0) {
        raise_ud_ctx(ctx);
        return false;
    }
    if (!allow_zeroing && evex_zeroing(prefix)) {
        raise_ud_ctx(ctx);
        return false;
    }
    return true;
}

static inline DecodedInstruction decode_evex_modrm_imm_instruction(
    CPU_CONTEXT* ctx,
    uint8_t* code,
    size_t code_size,
    uint8_t opcode,
    uint8_t map_select,
    uint8_t mandatory_prefix,
    int allowed_vector_mask,
    bool allow_writemask,
    bool allow_zeroing,
    EVEXDecodedInstructionInfo* info) {
    DecodedInstruction inst = {};
    if (!info) {
        raise_ud_ctx(ctx);
        return inst;
    }

    info->prefix = decode_evex_prefix(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    if (!evex_validate_common(ctx, &info->prefix, opcode, map_select, mandatory_prefix, allowed_vector_mask, allow_writemask, allow_zeroing, info)) {
        return inst;
    }

    apply_evex_state(ctx, &info->prefix);
    inst.address_size = ctx->cs.descriptor.long_mode ? 64 : 32;

    size_t offset = 5;
    decode_modrm_movdq(ctx, &inst, code, code_size, &offset, false);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    inst.opcode = opcode;
    inst.imm_size = 1;
    inst.immediate = code[offset++];
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}
