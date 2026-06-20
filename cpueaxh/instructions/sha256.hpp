#pragma once

// instrusments/sha256.hpp - SHA256RNDS2/SHA256MSG1/SHA256MSG2 implementation

#include "movdq.hpp"

static inline bool cpu_has_sha_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 29)) != 0;
}

static inline bool is_sha256rnds2_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x38 && code[prefix_len + 2] == 0xCB;
}

static inline bool is_sha256msg1_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x38 && code[prefix_len + 2] == 0xCC;
}

static inline bool is_sha256msg2_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x38 && code[prefix_len + 2] == 0xCD;
}

static inline uint32_t sha256_get_dword(XMMRegister value, int lane) {
    switch (lane) {
    case 0: return (uint32_t)(value.low & 0xFFFFFFFFu);
    case 1: return (uint32_t)(value.low >> 32);
    case 2: return (uint32_t)(value.high & 0xFFFFFFFFu);
    default: return (uint32_t)(value.high >> 32);
    }
}

static inline void sha256_set_dword(XMMRegister* value, int lane, uint32_t dword) {
    switch (lane) {
    case 0:
        value->low = (value->low & 0xFFFFFFFF00000000ULL) | dword;
        break;
    case 1:
        value->low = (value->low & 0x00000000FFFFFFFFULL) | ((uint64_t)dword << 32);
        break;
    case 2:
        value->high = (value->high & 0xFFFFFFFF00000000ULL) | dword;
        break;
    default:
        value->high = (value->high & 0x00000000FFFFFFFFULL) | ((uint64_t)dword << 32);
        break;
    }
}

static inline uint32_t sha256_rotr32(uint32_t value, int shift) {
    return (value >> shift) | (value << (32 - shift));
}

static inline uint32_t sha256_big_sigma0(uint32_t value) {
    return sha256_rotr32(value, 2) ^ sha256_rotr32(value, 13) ^ sha256_rotr32(value, 22);
}

static inline uint32_t sha256_big_sigma1(uint32_t value) {
    return sha256_rotr32(value, 6) ^ sha256_rotr32(value, 11) ^ sha256_rotr32(value, 25);
}

static inline uint32_t sha256_small_sigma0(uint32_t value) {
    return sha256_rotr32(value, 7) ^ sha256_rotr32(value, 18) ^ (value >> 3);
}

static inline uint32_t sha256_small_sigma1(uint32_t value) {
    return sha256_rotr32(value, 17) ^ sha256_rotr32(value, 19) ^ (value >> 10);
}

static inline uint32_t sha256_choose(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static inline uint32_t sha256_majority(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static inline DecodedInstruction decode_sha256_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t expected_opcode) {
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

    if (has_lock_prefix || has_unsupported_simd_prefix || mandatory_prefix != 0) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (offset + 4 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset++] != 0x0F || code[offset++] != 0x38) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode != expected_opcode) {
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

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, inst.inst_size);
    ctx->last_inst_size = inst.inst_size;
    return inst;
}

static inline XMMRegister read_sha256_source_operand(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (((inst->modrm >> 6) & 0x03) == 0x03) {
        return get_xmm128(ctx, decode_movdq_xmm_rm_index(ctx, inst->modrm));
    }
    return read_xmm_memory(ctx, inst->mem_address);
}

static inline XMMRegister apply_sha256rnds2(XMMRegister src1, XMMRegister src2, XMMRegister wk) {
    uint32_t a = sha256_get_dword(src2, 3);
    uint32_t b = sha256_get_dword(src2, 2);
    uint32_t c = sha256_get_dword(src1, 3);
    uint32_t d = sha256_get_dword(src1, 2);
    uint32_t e = sha256_get_dword(src2, 1);
    uint32_t f = sha256_get_dword(src2, 0);
    uint32_t g = sha256_get_dword(src1, 1);
    uint32_t h = sha256_get_dword(src1, 0);
    const uint32_t wk0 = sha256_get_dword(wk, 0);
    const uint32_t wk1 = sha256_get_dword(wk, 1);

    for (int round = 0; round < 2; ++round) {
        const uint32_t round_wk = round == 0 ? wk0 : wk1;
        const uint32_t t1 = h + sha256_big_sigma1(e) + sha256_choose(e, f, g) + round_wk;
        const uint32_t new_a = t1 + sha256_big_sigma0(a) + sha256_majority(a, b, c);
        const uint32_t new_e = t1 + d;
        h = g;
        g = f;
        f = e;
        e = new_e;
        d = c;
        c = b;
        b = a;
        a = new_a;
    }

    XMMRegister result = {};
    sha256_set_dword(&result, 3, a);
    sha256_set_dword(&result, 2, b);
    sha256_set_dword(&result, 1, e);
    sha256_set_dword(&result, 0, f);
    return result;
}

static inline XMMRegister apply_sha256msg1(XMMRegister src1, XMMRegister src2) {
    const uint32_t w4 = sha256_get_dword(src2, 0);
    const uint32_t w3 = sha256_get_dword(src1, 3);
    const uint32_t w2 = sha256_get_dword(src1, 2);
    const uint32_t w1 = sha256_get_dword(src1, 1);
    const uint32_t w0 = sha256_get_dword(src1, 0);
    XMMRegister result = {};
    sha256_set_dword(&result, 3, w3 + sha256_small_sigma0(w4));
    sha256_set_dword(&result, 2, w2 + sha256_small_sigma0(w3));
    sha256_set_dword(&result, 1, w1 + sha256_small_sigma0(w2));
    sha256_set_dword(&result, 0, w0 + sha256_small_sigma0(w1));
    return result;
}

static inline XMMRegister apply_sha256msg2(XMMRegister src1, XMMRegister src2) {
    const uint32_t w14 = sha256_get_dword(src2, 2);
    const uint32_t w15 = sha256_get_dword(src2, 3);
    const uint32_t w16 = sha256_get_dword(src1, 0) + sha256_small_sigma1(w14);
    const uint32_t w17 = sha256_get_dword(src1, 1) + sha256_small_sigma1(w15);
    const uint32_t w18 = sha256_get_dword(src1, 2) + sha256_small_sigma1(w16);
    const uint32_t w19 = sha256_get_dword(src1, 3) + sha256_small_sigma1(w17);
    XMMRegister result = {};
    sha256_set_dword(&result, 3, w19);
    sha256_set_dword(&result, 2, w18);
    sha256_set_dword(&result, 1, w17);
    sha256_set_dword(&result, 0, w16);
    return result;
}

static inline void execute_sha256rnds2(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    const DecodedInstruction inst = decode_sha256_instruction(ctx, code, code_size, 0xCB);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (!cpu_has_sha_feature()) {
        raise_ud_ctx(ctx);
        return;
    }

    const int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
    const XMMRegister src1 = get_xmm128(ctx, dest);
    const XMMRegister src2 = read_sha256_source_operand(ctx, &inst);
    if (cpu_has_exception(ctx)) {
        return;
    }
    set_xmm128(ctx, dest, apply_sha256rnds2(src1, src2, get_xmm128(ctx, 0)));
}

static inline void execute_sha256msg1(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    const DecodedInstruction inst = decode_sha256_instruction(ctx, code, code_size, 0xCC);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (!cpu_has_sha_feature()) {
        raise_ud_ctx(ctx);
        return;
    }

    const int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
    const XMMRegister src1 = get_xmm128(ctx, dest);
    const XMMRegister src2 = read_sha256_source_operand(ctx, &inst);
    if (cpu_has_exception(ctx)) {
        return;
    }
    set_xmm128(ctx, dest, apply_sha256msg1(src1, src2));
}

static inline void execute_sha256msg2(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    const DecodedInstruction inst = decode_sha256_instruction(ctx, code, code_size, 0xCD);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (!cpu_has_sha_feature()) {
        raise_ud_ctx(ctx);
        return;
    }

    const int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
    const XMMRegister src1 = get_xmm128(ctx, dest);
    const XMMRegister src2 = read_sha256_source_operand(ctx, &inst);
    if (cpu_has_exception(ctx)) {
        return;
    }
    set_xmm128(ctx, dest, apply_sha256msg2(src1, src2));
}
