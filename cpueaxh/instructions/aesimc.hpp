#pragma once

// instrusments/aesimc.hpp - AESIMC/VAESIMC implementation

#include "aesenc.hpp"

static XMMRegister apply_aesimc128(XMMRegister source) {
    return aesenc_m128i_to_xmm(_mm_aesimc_si128(aesenc_xmm_to_m128i(source)));
}

inline bool is_aesimc_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x38 && code[prefix_len + 2] == 0xDB;
}

inline DecodedInstruction decode_aesimc_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    return decode_aes_round_instruction(ctx, code, code_size, 0xDB);
}

inline void execute_aesimc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_aesimc_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    int dest = decode_aesenc_xmm_reg_index(ctx, inst.modrm);
    XMMRegister source = read_aesenc_source_operand(ctx, &inst);
    set_xmm128(ctx, dest, apply_aesimc128(source));
}
