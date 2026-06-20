#pragma once

// instructions/serialize.hpp - SERIALIZE instruction implementation.

#include "instruction_common.hpp"

static inline bool cpu_has_serialize_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[3] & (1 << 14)) != 0;
}

inline bool is_serialize_instruction(const uint8_t* code, size_t code_size, int prefix_len) {
    return code && prefix_len >= 0 && code_size >= (size_t)(prefix_len + 3) &&
        code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x01 && code[prefix_len + 2] == 0xE8;
}

static DecodedInstruction decode_serialize_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_simd_prefix = false;

    cpu_reset_prefix_state(ctx);
    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            has_simd_prefix = true;
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

    if (has_lock_prefix || has_simd_prefix || !cpu_has_serialize_feature()) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (offset + 3 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    if (code[offset++] != 0x0F || code[offset++] != 0x01 || code[offset++] != 0xE8) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.opcode = 0x01;
    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_serialize_with_decoded(CPU_CONTEXT* /*ctx*/, const DecodedInstruction* /*inst_ptr*/) {
    // Architectural serialization has no guest-visible register or flag result
    // in this single-threaded interpreter.
}

void execute_serialize(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_serialize_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_serialize_with_decoded(ctx, &inst);
}

inline void execute_serialize_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_serialize_with_decoded(ctx, &dec->cached);
}
