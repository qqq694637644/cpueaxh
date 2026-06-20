#pragma once

// instructions/avx_vex_common.hpp - Shared VEX prefix, register, and 128-bit helpers.

#include "crypto_instructions.hpp"

struct AVXRegister256 {
    XMMRegister low;
    XMMRegister high;
};

struct AVXVexPrefix {
    bool three_byte;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t opcode;
    size_t opcode_offset;
    size_t modrm_offset;
};

static void reset_avx_vex_state(CPU_CONTEXT* ctx) {
    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;
}

static AVXVexPrefix decode_avx_vex_prefix(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    if (code_size < 3) {
        raise_gp_ctx(ctx, 0);
    }

    AVXVexPrefix prefix = {};
    if (code[0] == 0xC5) {
        prefix.three_byte = false;
        prefix.byte2 = code[1];
        prefix.byte3 = code[1];
        prefix.opcode = code[2];
        prefix.opcode_offset = 2;
        prefix.modrm_offset = 3;
        return prefix;
    }

    if (code[0] == 0xC4) {
        if (code_size < 4) {
            raise_gp_ctx(ctx, 0);
        }
        prefix.three_byte = true;
        prefix.byte2 = code[1];
        prefix.byte3 = code[2];
        prefix.opcode = code[3];
        prefix.opcode_offset = 3;
        prefix.modrm_offset = 4;
        return prefix;
    }

    raise_ud_ctx(ctx);
    return prefix;
}

static uint8_t avx_vex_encoded_vvvv(const AVXVexPrefix* prefix) {
    uint8_t control = prefix->three_byte ? prefix->byte3 : prefix->byte2;
    return (control >> 3) & 0x0F;
}

static int avx_vex_source1_index(const AVXVexPrefix* prefix) {
    return (~avx_vex_encoded_vvvv(prefix)) & 0x0F;
}

static bool avx_vex_is_256(const AVXVexPrefix* prefix) {
    uint8_t control = prefix->three_byte ? prefix->byte3 : prefix->byte2;
    return (control & 0x04) != 0;
}

static uint8_t avx_vex_mandatory_prefix(const AVXVexPrefix* prefix) {
    uint8_t control = prefix->three_byte ? prefix->byte3 : prefix->byte2;
    return control & 0x03;
}

static uint8_t avx_vex_map_select(const AVXVexPrefix* prefix) {
    return prefix->three_byte ? (prefix->byte2 & 0x1F) : 0x01;
}

static void apply_avx_vex_state(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix) {
    reset_avx_vex_state(ctx);
    ctx->rex_present = true;
    ctx->rex_w = prefix->three_byte && ((prefix->byte3 & 0x80) != 0);
    ctx->rex_r = (prefix->byte2 & 0x80) == 0;
    ctx->rex_x = prefix->three_byte && ((prefix->byte2 & 0x40) == 0);
    ctx->rex_b = prefix->three_byte && ((prefix->byte2 & 0x20) == 0);
}

static bool avx_vex_requires_reserved_vvvv(const AVXVexPrefix* prefix) {
    return avx_vex_encoded_vvvv(prefix) != 0x0F;
}

static int avx_vex_dest_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    return decode_xorps_xmm_reg_index(ctx, modrm);
}

static int avx_vex_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    return decode_xorps_xmm_rm_index(ctx, modrm);
}

static int avx_vex_is4_source_index(CPU_CONTEXT* ctx, uint8_t imm8) {
    return ctx->cs.descriptor.long_mode ? ((imm8 >> 4) & 0x0F) : ((imm8 >> 4) & 0x07);
}

static void clear_all_avx_registers(CPU_CONTEXT* ctx) {
    CPUEAXH_MEMSET(ctx->xmm, 0, sizeof(ctx->xmm));
    clear_all_ymm_upper128(ctx);
}

static AVXRegister256 get_ymm256(CPU_CONTEXT* ctx, int reg) {
    AVXRegister256 value = {};
    value.low = get_xmm128(ctx, reg);
    value.high = get_ymm_upper128(ctx, reg);
    return value;
}

static void set_ymm256(CPU_CONTEXT* ctx, int reg, AVXRegister256 value) {
    set_xmm128(ctx, reg, value.low);
    set_ymm_upper128(ctx, reg, value.high);
    clear_zmm_upper256(ctx, reg);
}

static AVXRegister256 read_ymm_memory(CPU_CONTEXT* ctx, uint64_t address) {
    AVXRegister256 value = {};
    value.low = read_xmm_memory(ctx, address);
    value.high = read_xmm_memory(ctx, address + 16);
    return value;
}

static void write_ymm_memory(CPU_CONTEXT* ctx, uint64_t address, AVXRegister256 value) {
    ZMMUpperRegister packed = {};
    packed.lower = value.low;
    packed.upper = value.high;
    write_zmm_upper_memory(ctx, address, packed);
}

static uint32_t get_ymm_lane_bits(const AVXRegister256& value, int lane) {
    if (lane < 4) {
        return get_xmm_lane_bits(value.low, lane);
    }
    return get_xmm_lane_bits(value.high, lane - 4);
}

static void set_ymm_lane_bits(AVXRegister256* value, int lane, uint32_t bits) {
    if (lane < 4) {
        set_xmm_lane_bits(&value->low, lane, bits);
        return;
    }
    set_xmm_lane_bits(&value->high, lane - 4, bits);
}

static uint64_t get_ymm_pd_lane_bits(const AVXRegister256& value, int lane) {
    if (lane < 2) {
        return get_sse2_arith_pd_lane_bits(value.low, lane);
    }
    return get_sse2_arith_pd_lane_bits(value.high, lane - 2);
}

static void set_ymm_pd_lane_bits(AVXRegister256* value, int lane, uint64_t bits) {
    if (lane < 2) {
        set_sse2_arith_pd_lane_bits(&value->low, lane, bits);
        return;
    }
    set_sse2_arith_pd_lane_bits(&value->high, lane - 2, bits);
}

static __m128i avx_xmm_to_m128i(XMMRegister value) {
    return _mm_set_epi64x((long long)value.high, (long long)value.low);
}

static XMMRegister avx_m128i_to_xmm(__m128i value) {
    XMMRegister result = {};
    alignas(16) uint64_t qwords[2] = {};
    _mm_storeu_si128((__m128i*)qwords, value);
    result.low = qwords[0];
    result.high = qwords[1];
    return result;
}

static void update_avx_pcmpstr_flags(CPU_CONTEXT* ctx, bool cf, bool zf, bool sf, bool of) {
    ctx->rflags &= ~(RFLAGS_CF | RFLAGS_PF | RFLAGS_AF | RFLAGS_ZF | RFLAGS_SF | RFLAGS_OF);
    if (cf) {
        ctx->rflags |= RFLAGS_CF;
    }
    if (zf) {
        ctx->rflags |= RFLAGS_ZF;
    }
    if (sf) {
        ctx->rflags |= RFLAGS_SF;
    }
    if (of) {
        ctx->rflags |= RFLAGS_OF;
    }
}

static int avx_pcmpestr_length(CPU_CONTEXT* ctx, int reg_index, bool rex_w, uint8_t imm8) {
    int max_length = (imm8 & 0x01) != 0 ? 8 : 16;
    int64_t raw_length = rex_w ? (int64_t)get_reg64(ctx, reg_index)
                               : (int64_t)(int32_t)get_reg32(ctx, reg_index);
    if (raw_length > max_length) {
        return max_length;
    }
    if (raw_length < -max_length) {
        return -max_length;
    }
    return (int)raw_length;
}

struct AVXPcmpstrResult {
    uint32_t index;
    XMMRegister mask;
    bool cf;
    bool zf;
    bool sf;
    bool of;
};

static AVXPcmpstrResult avx_execute_pcmpestr(__m128i lhs, int length_a, __m128i rhs, int length_b, uint8_t imm8) {
    AVXPcmpstrResult result = {};

#define AVX_PCMPESTR_CASE(mode) \
    case mode: { \
        result.index = (uint32_t)_mm_cmpestri(lhs, length_a, rhs, length_b, mode); \
        result.mask = avx_m128i_to_xmm(_mm_cmpestrm(lhs, length_a, rhs, length_b, mode)); \
        result.cf = _mm_cmpestrc(lhs, length_a, rhs, length_b, mode) != 0; \
        result.zf = _mm_cmpestrz(lhs, length_a, rhs, length_b, mode) != 0; \
        result.sf = _mm_cmpestrs(lhs, length_a, rhs, length_b, mode) != 0; \
        result.of = _mm_cmpestro(lhs, length_a, rhs, length_b, mode) != 0; \
        return result; \
    }
#define AVX_PCMPESTR_CASE16(prefix) \
    AVX_PCMPESTR_CASE((prefix) | 0x0) \
    AVX_PCMPESTR_CASE((prefix) | 0x1) \
    AVX_PCMPESTR_CASE((prefix) | 0x2) \
    AVX_PCMPESTR_CASE((prefix) | 0x3) \
    AVX_PCMPESTR_CASE((prefix) | 0x4) \
    AVX_PCMPESTR_CASE((prefix) | 0x5) \
    AVX_PCMPESTR_CASE((prefix) | 0x6) \
    AVX_PCMPESTR_CASE((prefix) | 0x7) \
    AVX_PCMPESTR_CASE((prefix) | 0x8) \
    AVX_PCMPESTR_CASE((prefix) | 0x9) \
    AVX_PCMPESTR_CASE((prefix) | 0xA) \
    AVX_PCMPESTR_CASE((prefix) | 0xB) \
    AVX_PCMPESTR_CASE((prefix) | 0xC) \
    AVX_PCMPESTR_CASE((prefix) | 0xD) \
    AVX_PCMPESTR_CASE((prefix) | 0xE) \
    AVX_PCMPESTR_CASE((prefix) | 0xF)

    switch (imm8) {
        AVX_PCMPESTR_CASE16(0x00);
        AVX_PCMPESTR_CASE16(0x10);
        AVX_PCMPESTR_CASE16(0x20);
        AVX_PCMPESTR_CASE16(0x30);
        AVX_PCMPESTR_CASE16(0x40);
        AVX_PCMPESTR_CASE16(0x50);
        AVX_PCMPESTR_CASE16(0x60);
        AVX_PCMPESTR_CASE16(0x70);
        AVX_PCMPESTR_CASE16(0x80);
        AVX_PCMPESTR_CASE16(0x90);
        AVX_PCMPESTR_CASE16(0xA0);
        AVX_PCMPESTR_CASE16(0xB0);
        AVX_PCMPESTR_CASE16(0xC0);
        AVX_PCMPESTR_CASE16(0xD0);
        AVX_PCMPESTR_CASE16(0xE0);
        AVX_PCMPESTR_CASE16(0xF0);
    default:
        return result;
    }

#undef AVX_PCMPESTR_CASE16
#undef AVX_PCMPESTR_CASE
}

static AVXPcmpstrResult avx_execute_pcmpistr(__m128i lhs, __m128i rhs, uint8_t imm8) {
    AVXPcmpstrResult result = {};

#define AVX_PCMPISTR_CASE(mode) \
    case mode: { \
        result.index = (uint32_t)_mm_cmpistri(lhs, rhs, mode); \
        result.mask = avx_m128i_to_xmm(_mm_cmpistrm(lhs, rhs, mode)); \
        result.cf = _mm_cmpistrc(lhs, rhs, mode) != 0; \
        result.zf = _mm_cmpistrz(lhs, rhs, mode) != 0; \
        result.sf = _mm_cmpistrs(lhs, rhs, mode) != 0; \
        result.of = _mm_cmpistro(lhs, rhs, mode) != 0; \
        return result; \
    }
#define AVX_PCMPISTR_CASE16(prefix) \
    AVX_PCMPISTR_CASE((prefix) | 0x0) \
    AVX_PCMPISTR_CASE((prefix) | 0x1) \
    AVX_PCMPISTR_CASE((prefix) | 0x2) \
    AVX_PCMPISTR_CASE((prefix) | 0x3) \
    AVX_PCMPISTR_CASE((prefix) | 0x4) \
    AVX_PCMPISTR_CASE((prefix) | 0x5) \
    AVX_PCMPISTR_CASE((prefix) | 0x6) \
    AVX_PCMPISTR_CASE((prefix) | 0x7) \
    AVX_PCMPISTR_CASE((prefix) | 0x8) \
    AVX_PCMPISTR_CASE((prefix) | 0x9) \
    AVX_PCMPISTR_CASE((prefix) | 0xA) \
    AVX_PCMPISTR_CASE((prefix) | 0xB) \
    AVX_PCMPISTR_CASE((prefix) | 0xC) \
    AVX_PCMPISTR_CASE((prefix) | 0xD) \
    AVX_PCMPISTR_CASE((prefix) | 0xE) \
    AVX_PCMPISTR_CASE((prefix) | 0xF)

    switch (imm8) {
        AVX_PCMPISTR_CASE16(0x00);
        AVX_PCMPISTR_CASE16(0x10);
        AVX_PCMPISTR_CASE16(0x20);
        AVX_PCMPISTR_CASE16(0x30);
        AVX_PCMPISTR_CASE16(0x40);
        AVX_PCMPISTR_CASE16(0x50);
        AVX_PCMPISTR_CASE16(0x60);
        AVX_PCMPISTR_CASE16(0x70);
        AVX_PCMPISTR_CASE16(0x80);
        AVX_PCMPISTR_CASE16(0x90);
        AVX_PCMPISTR_CASE16(0xA0);
        AVX_PCMPISTR_CASE16(0xB0);
        AVX_PCMPISTR_CASE16(0xC0);
        AVX_PCMPISTR_CASE16(0xD0);
        AVX_PCMPISTR_CASE16(0xE0);
        AVX_PCMPISTR_CASE16(0xF0);
    default:
        return result;
    }

#undef AVX_PCMPISTR_CASE16
#undef AVX_PCMPISTR_CASE
}

static void validate_avx_movaps_alignment(CPU_CONTEXT* ctx, const DecodedInstruction* inst, bool is_256) {
    if (((inst->modrm >> 6) & 0x03) == 3) {
        return;
    }
    uint64_t mask = is_256 ? 0x1FULL : 0x0FULL;
    if ((inst->mem_address & mask) != 0) {
        raise_gp_ctx(ctx, 0);
    }
}

static uint32_t compute_avx_movmskpd128(XMMRegister value) {
    return (uint32_t)((value.low >> 63) & 0x1ULL)
        | (uint32_t)(((value.high >> 63) & 0x1ULL) << 1);
}

static uint32_t compute_avx_movmskpd256(AVXRegister256 value) {
    return compute_avx_movmskpd128(value.low)
        | (uint32_t)(((value.high.low >> 63) & 0x1ULL) << 2)
        | (uint32_t)(((value.high.high >> 63) & 0x1ULL) << 3);
}

static uint32_t compute_avx_movmskps128(XMMRegister value) {
    uint32_t mask = 0;
    for (int lane = 0; lane < 4; lane++) {
        mask |= ((get_xmm_lane_bits(value, lane) >> 31) & 0x1U) << lane;
    }
    return mask;
}

static uint32_t compute_avx_movmskps256(AVXRegister256 value) {
    uint32_t mask = 0;
    for (int lane = 0; lane < 8; lane++) {
        mask |= ((get_ymm_lane_bits(value, lane) >> 31) & 0x1U) << lane;
    }
    return mask;
}

static uint32_t compute_avx_pmovmskb128(XMMRegister value) {
    uint8_t bytes[16] = {};
    uint32_t mask = 0;
    sse2_misc_xmm_to_bytes(value, bytes);
    for (int index = 0; index < 16; index++) {
        mask |= (uint32_t)((bytes[index] >> 7) & 0x1U) << index;
    }
    return mask;
}

static uint32_t compute_avx_pmovmskb256(AVXRegister256 value) {
    uint8_t low_bytes[16] = {};
    uint8_t high_bytes[16] = {};
    uint32_t mask = 0;
    sse2_misc_xmm_to_bytes(value.low, low_bytes);
    sse2_misc_xmm_to_bytes(value.high, high_bytes);
    for (int index = 0; index < 16; index++) {
        mask |= (uint32_t)((low_bytes[index] >> 7) & 0x1U) << index;
        mask |= (uint32_t)((high_bytes[index] >> 7) & 0x1U) << (index + 16);
    }
    return mask;
}

static XMMRegister read_avx_int_rm128(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, avx_vex_rm_index(ctx, inst->modrm));
    }
    return read_xmm_memory(ctx, inst->mem_address);
}

static XMMRegister read_avx_movdqa_rm128(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    validate_avx_movaps_alignment(ctx, inst, false);
    return read_avx_int_rm128(ctx, inst);
}

static void write_avx_int_rm128(CPU_CONTEXT* ctx, const DecodedInstruction* inst, XMMRegister value, bool clear_upper) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    int dest = avx_vex_rm_index(ctx, inst->modrm);
    if (mod == 3) {
        set_xmm128(ctx, dest, value);
        if (clear_upper) {
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }
    write_xmm_memory(ctx, inst->mem_address, value);
}

static void write_avx_movdqa_rm128(CPU_CONTEXT* ctx, const DecodedInstruction* inst, XMMRegister value, bool clear_upper) {
    validate_avx_movaps_alignment(ctx, inst, false);
    write_avx_int_rm128(ctx, inst, value, clear_upper);
}

