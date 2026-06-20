#pragma once

// instructions/avx_vex_ops.hpp - AVX/VEX operation helpers shared by the executor.

#include "avx_vex_decode.hpp"

static AVXRegister256 read_avx_rm256(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_ymm256(ctx, avx_vex_rm_index(ctx, inst->modrm));
    }
    return read_ymm_memory(ctx, inst->mem_address);
}

static void write_avx_rm256(CPU_CONTEXT* ctx, const DecodedInstruction* inst, AVXRegister256 value) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        set_ymm256(ctx, avx_vex_rm_index(ctx, inst->modrm), value);
        return;
    }
    write_ymm_memory(ctx, inst->mem_address, value);
}

static AVXRegister256 read_avx_movaps_rm256(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_ymm256(ctx, avx_vex_rm_index(ctx, inst->modrm));
    }
    validate_avx_movaps_alignment(ctx, inst, true);
    return read_ymm_memory(ctx, inst->mem_address);
}

static void write_avx_movaps_rm256(CPU_CONTEXT* ctx, const DecodedInstruction* inst, AVXRegister256 value) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        set_ymm256(ctx, avx_vex_rm_index(ctx, inst->modrm), value);
        return;
    }
    validate_avx_movaps_alignment(ctx, inst, true);
    write_ymm_memory(ctx, inst->mem_address, value);
}

static XMMRegister apply_avx_logic128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    switch (opcode) {
    case 0x54:
        result.low = lhs.low & rhs.low;
        result.high = lhs.high & rhs.high;
        break;
    case 0x55:
        result.low = (~lhs.low) & rhs.low;
        result.high = (~lhs.high) & rhs.high;
        break;
    case 0x56:
        result.low = lhs.low | rhs.low;
        result.high = lhs.high | rhs.high;
        break;
    case 0x57:
        result.low = lhs.low ^ rhs.low;
        result.high = lhs.high ^ rhs.high;
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }
    return result;
}

static AVXRegister256 apply_avx_logic256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_logic128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx_logic128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_unpack_ps128(uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    int base = (opcode == 0x15) ? 2 : 0;
    set_sse_shuffle_lane_bits(&result, 0, get_sse_shuffle_lane_bits(lhs, base + 0));
    set_sse_shuffle_lane_bits(&result, 1, get_sse_shuffle_lane_bits(rhs, base + 0));
    set_sse_shuffle_lane_bits(&result, 2, get_sse_shuffle_lane_bits(lhs, base + 1));
    set_sse_shuffle_lane_bits(&result, 3, get_sse_shuffle_lane_bits(rhs, base + 1));
    return result;
}

static AVXRegister256 apply_avx_unpack_ps256(uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    int lane_offset = (opcode == 0x15) ? 2 : 0;
    for (int block = 0; block < 2; ++block) {
        int base = block * 4;
        set_ymm_lane_bits(&result, base + 0, get_ymm_lane_bits(lhs, base + lane_offset + 0));
        set_ymm_lane_bits(&result, base + 1, get_ymm_lane_bits(rhs, base + lane_offset + 0));
        set_ymm_lane_bits(&result, base + 2, get_ymm_lane_bits(lhs, base + lane_offset + 1));
        set_ymm_lane_bits(&result, base + 3, get_ymm_lane_bits(rhs, base + lane_offset + 1));
    }
    return result;
}

static XMMRegister apply_avx_unpack_pd128(uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    int lane = (opcode == 0x15) ? 1 : 0;
    set_sse2_arith_pd_lane_bits(&result, 0, get_sse2_arith_pd_lane_bits(lhs, lane));
    set_sse2_arith_pd_lane_bits(&result, 1, get_sse2_arith_pd_lane_bits(rhs, lane));
    return result;
}

static AVXRegister256 apply_avx_unpack_pd256(uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    int lane = (opcode == 0x15) ? 1 : 0;
    for (int block = 0; block < 2; ++block) {
        int base = block * 2;
        set_ymm_pd_lane_bits(&result, base + 0, get_ymm_pd_lane_bits(lhs, base + lane));
        set_ymm_pd_lane_bits(&result, base + 1, get_ymm_pd_lane_bits(rhs, base + lane));
    }
    return result;
}

static XMMRegister apply_avx_shufps128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    XMMRegister result = {};
    set_sse_shuffle_lane_bits(&result, 0, get_sse_shuffle_lane_bits(lhs, imm8 & 0x03));
    set_sse_shuffle_lane_bits(&result, 1, get_sse_shuffle_lane_bits(lhs, (imm8 >> 2) & 0x03));
    set_sse_shuffle_lane_bits(&result, 2, get_sse_shuffle_lane_bits(rhs, (imm8 >> 4) & 0x03));
    set_sse_shuffle_lane_bits(&result, 3, get_sse_shuffle_lane_bits(rhs, (imm8 >> 6) & 0x03));
    return result;
}

static AVXRegister256 apply_avx_shufps256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t imm8) {
    AVXRegister256 result = {};
    for (int block = 0; block < 2; ++block) {
        int base = block * 4;
        set_ymm_lane_bits(&result, base + 0, get_ymm_lane_bits(lhs, base + (imm8 & 0x03)));
        set_ymm_lane_bits(&result, base + 1, get_ymm_lane_bits(lhs, base + ((imm8 >> 2) & 0x03)));
        set_ymm_lane_bits(&result, base + 2, get_ymm_lane_bits(rhs, base + ((imm8 >> 4) & 0x03)));
        set_ymm_lane_bits(&result, base + 3, get_ymm_lane_bits(rhs, base + ((imm8 >> 6) & 0x03)));
    }
    return result;
}

static XMMRegister apply_avx_shufpd128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    XMMRegister result = {};
    set_sse2_arith_pd_lane_bits(&result, 0, get_sse2_arith_pd_lane_bits(lhs, imm8 & 0x01));
    set_sse2_arith_pd_lane_bits(&result, 1, get_sse2_arith_pd_lane_bits(rhs, (imm8 >> 1) & 0x01));
    return result;
}

static AVXRegister256 apply_avx_shufpd256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t imm8) {
    AVXRegister256 result = {};
    set_ymm_pd_lane_bits(&result, 0, get_ymm_pd_lane_bits(lhs, imm8 & 0x01));
    set_ymm_pd_lane_bits(&result, 1, get_ymm_pd_lane_bits(rhs, (imm8 >> 1) & 0x01));
    set_ymm_pd_lane_bits(&result, 2, get_ymm_pd_lane_bits(lhs, 2 + ((imm8 >> 2) & 0x01)));
    set_ymm_pd_lane_bits(&result, 3, get_ymm_pd_lane_bits(rhs, 2 + ((imm8 >> 3) & 0x01)));
    return result;
}

static XMMRegister apply_avx_cvtps2pd128(uint64_t packed) {
    XMMRegister result = {};
    float lane0 = sse2_convert_bits_to_float((uint32_t)(packed & 0xFFFFFFFFU));
    float lane1 = sse2_convert_bits_to_float((uint32_t)(packed >> 32));
    result.low = sse2_convert_double_to_bits((double)lane0);
    result.high = sse2_convert_double_to_bits((double)lane1);
    return result;
}

static AVXRegister256 apply_avx_cvtps2pd256(XMMRegister source) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 4; ++lane) {
        float value = sse2_convert_bits_to_float(sse2_convert_get_ps_lane_bits(source, lane));
        set_ymm_pd_lane_bits(&result, lane, sse2_convert_double_to_bits((double)value));
    }
    return result;
}

static XMMRegister apply_avx_cvtpd2ps128(XMMRegister source) {
    XMMRegister result = {};
    sse2_convert_set_ps_lane_bits(&result, 0, sse2_convert_float_to_bits((float)sse2_convert_bits_to_double(source.low)));
    sse2_convert_set_ps_lane_bits(&result, 1, sse2_convert_float_to_bits((float)sse2_convert_bits_to_double(source.high)));
    return result;
}

static XMMRegister apply_avx_cvtpd2ps256(AVXRegister256 source) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; ++lane) {
        double value = sse2_convert_bits_to_double(get_ymm_pd_lane_bits(source, lane));
        sse2_convert_set_ps_lane_bits(&result, lane, sse2_convert_float_to_bits((float)value));
    }
    return result;
}

static XMMRegister apply_avx_cvtdq2ps128(XMMRegister source) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; ++lane) {
        int32_t value = sse2_convert_bits_to_i32(sse2_convert_get_ps_lane_bits(source, lane));
        sse2_convert_set_ps_lane_bits(&result, lane, sse2_convert_float_to_bits((float)value));
    }
    return result;
}

static AVXRegister256 apply_avx_cvtdq2ps256(AVXRegister256 source) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; ++lane) {
        int32_t value = (int32_t)get_ymm_lane_bits(source, lane);
        set_ymm_lane_bits(&result, lane, sse2_convert_float_to_bits((float)value));
    }
    return result;
}

static XMMRegister apply_avx_cvtps2dq128(XMMRegister source, bool truncate, uint32_t mxcsr) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; ++lane) {
        double value = (double)sse2_convert_bits_to_float(sse2_convert_get_ps_lane_bits(source, lane));
        int64_t converted = 0;
        bool success = sse2_convert_round_fp_to_integer(value, 32, truncate, mxcsr, &converted);
        sse2_convert_set_ps_lane_bits(&result, lane, success ? (uint32_t)((int32_t)converted) : 0x80000000U);
    }
    return result;
}

static AVXRegister256 apply_avx_cvtps2dq256(AVXRegister256 source, bool truncate, uint32_t mxcsr) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; ++lane) {
        double value = (double)sse2_convert_bits_to_float(get_ymm_lane_bits(source, lane));
        int64_t converted = 0;
        bool success = sse2_convert_round_fp_to_integer(value, 32, truncate, mxcsr, &converted);
        set_ymm_lane_bits(&result, lane, success ? (uint32_t)((int32_t)converted) : 0x80000000U);
    }
    return result;
}

static XMMRegister apply_avx_cvtdq2pd128(uint64_t packed) {
    XMMRegister result = {};
    int32_t lane0 = (int32_t)(packed & 0xFFFFFFFFU);
    int32_t lane1 = (int32_t)(packed >> 32);
    result.low = sse2_convert_double_to_bits((double)lane0);
    result.high = sse2_convert_double_to_bits((double)lane1);
    return result;
}

static AVXRegister256 apply_avx_cvtdq2pd256(XMMRegister source) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 4; ++lane) {
        int32_t value = sse2_convert_bits_to_i32(sse2_convert_get_ps_lane_bits(source, lane));
        set_ymm_pd_lane_bits(&result, lane, sse2_convert_double_to_bits((double)value));
    }
    return result;
}

static XMMRegister apply_avx_cvtpd2dq128(XMMRegister source, bool truncate, uint32_t mxcsr) {
    XMMRegister result = {};
    for (int lane = 0; lane < 2; ++lane) {
        double value = sse2_convert_bits_to_double(sse2_convert_get_pd_lane_bits(source, lane));
        int64_t converted = 0;
        bool success = sse2_convert_round_fp_to_integer(value, 32, truncate, mxcsr, &converted);
        sse2_convert_set_ps_lane_bits(&result, lane, success ? (uint32_t)((int32_t)converted) : 0x80000000U);
    }
    return result;
}

static XMMRegister apply_avx_cvtpd2dq256(AVXRegister256 source, bool truncate, uint32_t mxcsr) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; ++lane) {
        double value = sse2_convert_bits_to_double(get_ymm_pd_lane_bits(source, lane));
        int64_t converted = 0;
        bool success = sse2_convert_round_fp_to_integer(value, 32, truncate, mxcsr, &converted);
        sse2_convert_set_ps_lane_bits(&result, lane, success ? (uint32_t)((int32_t)converted) : 0x80000000U);
    }
    return result;
}

static XMMRegister apply_avx_cvtss2sd128(XMMRegister src1, uint32_t scalar_bits) {
    XMMRegister result = src1;
    result.low = sse2_convert_double_to_bits((double)sse_convert_bits_to_float(scalar_bits));
    return result;
}

static XMMRegister apply_avx_cvtsd2ss128(XMMRegister src1, uint64_t scalar_bits) {
    XMMRegister result = src1;
    set_xmm_lane_bits(&result, 0, sse_convert_float_to_bits((float)sse2_convert_bits_to_double(scalar_bits)));
    return result;
}

static XMMRegister apply_avx_cvtsi2ss128(XMMRegister src1, int64_t source_value) {
    XMMRegister result = src1;
    set_xmm_lane_bits(&result, 0, sse_convert_float_to_bits((float)source_value));
    return result;
}

static XMMRegister apply_avx_cvtsi2sd128(XMMRegister src1, int64_t source_value) {
    XMMRegister result = src1;
    result.low = sse2_convert_double_to_bits((double)source_value);
    return result;
}

static bool evaluate_avx_cmp_predicate_common(bool unordered, bool equal, bool less, bool greater, uint8_t predicate) {
    bool ordered = !unordered;
    bool less_equal = ordered && (less || equal);
    bool greater_equal = ordered && (greater || equal);
    bool not_equal = ordered && !equal;

    switch (predicate & 0x1F) {
    case 0x00: return ordered && equal;
    case 0x01: return ordered && less;
    case 0x02: return less_equal;
    case 0x03: return unordered;
    case 0x04: return unordered || not_equal;
    case 0x05: return unordered || !less;
    case 0x06: return unordered || !less_equal;
    case 0x07: return ordered;
    case 0x08: return unordered || equal;
    case 0x09: return unordered || !greater_equal;
    case 0x0A: return unordered || !greater;
    case 0x0B: return false;
    case 0x0C: return ordered && !equal;
    case 0x0D: return greater_equal;
    case 0x0E: return ordered && greater;
    case 0x0F: return true;
    case 0x10: return ordered && equal;
    case 0x11: return ordered && less;
    case 0x12: return less_equal;
    case 0x13: return unordered;
    case 0x14: return unordered || not_equal;
    case 0x15: return unordered || !less;
    case 0x16: return unordered || !less_equal;
    case 0x17: return ordered;
    case 0x18: return unordered || equal;
    case 0x19: return unordered || !greater_equal;
    case 0x1A: return unordered || !greater;
    case 0x1B: return false;
    case 0x1C: return ordered && !equal;
    case 0x1D: return greater_equal;
    case 0x1E: return ordered && greater;
    case 0x1F: return true;
    default:   return false;
    }
}

static bool evaluate_avx_cmp_predicate_ps(float lhs, float rhs, uint8_t predicate) {
    bool unordered = sse_cmp_is_nan(lhs) || sse_cmp_is_nan(rhs);
    bool equal = !unordered && lhs == rhs;
    bool less = !unordered && lhs < rhs;
    bool greater = !unordered && lhs > rhs;
    return evaluate_avx_cmp_predicate_common(unordered, equal, less, greater, predicate);
}

static bool evaluate_avx_cmp_predicate_pd(double lhs, double rhs, uint8_t predicate) {
    bool unordered = sse2_cmp_pd_is_nan(lhs) || sse2_cmp_pd_is_nan(rhs);
    bool equal = !unordered && lhs == rhs;
    bool less = !unordered && lhs < rhs;
    bool greater = !unordered && lhs > rhs;
    return evaluate_avx_cmp_predicate_common(unordered, equal, less, greater, predicate);
}

static XMMRegister apply_avx_cmpps128(XMMRegister lhs, XMMRegister rhs, uint8_t predicate) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        float lhs_lane = sse_cmp_bits_to_float(get_xmm_lane_bits(lhs, lane));
        float rhs_lane = sse_cmp_bits_to_float(get_xmm_lane_bits(rhs, lane));
        set_xmm_lane_bits(&result, lane, evaluate_avx_cmp_predicate_ps(lhs_lane, rhs_lane, predicate) ? 0xFFFFFFFFU : 0x00000000U);
    }
    return result;
}

static AVXRegister256 apply_avx_cmpps256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t predicate) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        float lhs_lane = sse_cmp_bits_to_float(get_ymm_lane_bits(lhs, lane));
        float rhs_lane = sse_cmp_bits_to_float(get_ymm_lane_bits(rhs, lane));
        set_ymm_lane_bits(&result, lane, evaluate_avx_cmp_predicate_ps(lhs_lane, rhs_lane, predicate) ? 0xFFFFFFFFU : 0x00000000U);
    }
    return result;
}

static XMMRegister apply_avx_cmppd128(XMMRegister lhs, XMMRegister rhs, uint8_t predicate) {
    XMMRegister result = {};
    for (int lane = 0; lane < 2; lane++) {
        double lhs_lane = sse2_cmp_pd_bits_to_double(get_sse2_arith_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_cmp_pd_bits_to_double(get_sse2_arith_pd_lane_bits(rhs, lane));
        set_sse2_arith_pd_lane_bits(&result, lane, evaluate_avx_cmp_predicate_pd(lhs_lane, rhs_lane, predicate) ? 0xFFFFFFFFFFFFFFFFULL : 0x0000000000000000ULL);
    }
    return result;
}

static AVXRegister256 apply_avx_cmppd256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t predicate) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 4; lane++) {
        double lhs_lane = sse2_cmp_pd_bits_to_double(get_ymm_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_cmp_pd_bits_to_double(get_ymm_pd_lane_bits(rhs, lane));
        set_ymm_pd_lane_bits(&result, lane, evaluate_avx_cmp_predicate_pd(lhs_lane, rhs_lane, predicate) ? 0xFFFFFFFFFFFFFFFFULL : 0x0000000000000000ULL);
    }
    return result;
}

static XMMRegister apply_avx_cmpss128(XMMRegister src1, uint32_t rhs_bits, uint8_t predicate) {
    XMMRegister result = src1;
    float lhs = sse_cmp_bits_to_float(get_xmm_lane_bits(src1, 0));
    float rhs = sse_cmp_bits_to_float(rhs_bits);
    set_xmm_lane_bits(&result, 0, evaluate_avx_cmp_predicate_ps(lhs, rhs, predicate) ? 0xFFFFFFFFU : 0x00000000U);
    return result;
}

static XMMRegister apply_avx_cmpsd128(XMMRegister src1, uint64_t rhs_bits, uint8_t predicate) {
    XMMRegister result = src1;
    double lhs = sse2_cmp_pd_bits_to_double(src1.low);
    double rhs = sse2_cmp_pd_bits_to_double(rhs_bits);
    result.low = evaluate_avx_cmp_predicate_pd(lhs, rhs, predicate) ? 0xFFFFFFFFFFFFFFFFULL : 0x0000000000000000ULL;
    return result;
}

static XMMRegister apply_avx_arith128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        float lhs_lane = sse_bits_to_float(get_xmm_lane_bits(lhs, lane));
        float rhs_lane = sse_bits_to_float(get_xmm_lane_bits(rhs, lane));
        set_xmm_lane_bits(&result, lane, sse_float_to_bits(apply_sse_arith_scalar(ctx, opcode, lhs_lane, rhs_lane)));
    }
    return result;
}

static AVXRegister256 apply_avx_arith256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        float lhs_lane = sse_bits_to_float(get_ymm_lane_bits(lhs, lane));
        float rhs_lane = sse_bits_to_float(get_ymm_lane_bits(rhs, lane));
        set_ymm_lane_bits(&result, lane, sse_float_to_bits(apply_sse_arith_scalar(ctx, opcode, lhs_lane, rhs_lane)));
    }
    return result;
}

static XMMRegister apply_avx_minmax128(uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    __m128 lhs_vec = sse_math_xmm_to_m128(lhs);
    __m128 rhs_vec = sse_math_xmm_to_m128(rhs);
    return sse_math_m128_to_xmm(opcode == 0x5D ? _mm_min_ps(lhs_vec, rhs_vec)
                                               : _mm_max_ps(lhs_vec, rhs_vec));
}

static AVXRegister256 apply_avx_minmax256(uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_minmax128(opcode, lhs.low, rhs.low);
    result.high = apply_avx_minmax128(opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_sqrt128(XMMRegister rhs) {
    return sse_math_m128_to_xmm(_mm_sqrt_ps(sse_math_xmm_to_m128(rhs)));
}

static AVXRegister256 apply_avx_sqrt256(AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_sqrt128(rhs.low);
    result.high = apply_avx_sqrt128(rhs.high);
    return result;
}

static XMMRegister apply_avx_unary_math128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister rhs) {
    __m128 rhs_vec = sse_math_xmm_to_m128(rhs);
    switch (opcode) {
    case 0x51:
        return sse_math_m128_to_xmm(_mm_sqrt_ps(rhs_vec));
    case 0x52:
        return sse_math_m128_to_xmm(_mm_rsqrt_ps(rhs_vec));
    case 0x53:
        return sse_math_m128_to_xmm(_mm_rcp_ps(rhs_vec));
    default:
        raise_ud_ctx(ctx);
        return rhs;
    }
}

static AVXRegister256 apply_avx_unary_math256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_unary_math128(ctx, opcode, rhs.low);
    result.high = apply_avx_unary_math128(ctx, opcode, rhs.high);
    return result;
}

static XMMRegister apply_avx_unary_math_ss128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister src1, uint32_t rhs_bits) {
    XMMRegister result = src1;
    XMMRegister rhs_scalar = {};
    set_xmm_lane_bits(&rhs_scalar, 0, rhs_bits);
    __m128 rhs_vec = sse_math_xmm_to_m128(rhs_scalar);
    float low_result = 0.0f;

    switch (opcode) {
    case 0x51:
        low_result = _mm_cvtss_f32(_mm_sqrt_ss(rhs_vec));
        break;
    case 0x52:
        low_result = _mm_cvtss_f32(_mm_rsqrt_ss(rhs_vec));
        break;
    case 0x53:
        low_result = _mm_cvtss_f32(_mm_rcp_ss(rhs_vec));
        break;
    default:
        raise_ud_ctx(ctx);
    }

    set_xmm_lane_bits(&result, 0, sse_math_float_to_bits(low_result));
    return result;
}

static XMMRegister apply_avx_haddhsub_ps128(uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    __m128 lhs_vec = sse_math_xmm_to_m128(lhs);
    __m128 rhs_vec = sse_math_xmm_to_m128(rhs);
    return sse_math_m128_to_xmm(opcode == 0x7C ? _mm_hadd_ps(lhs_vec, rhs_vec)
                                                   : _mm_hsub_ps(lhs_vec, rhs_vec));
}

static AVXRegister256 apply_avx_haddhsub_ps256(uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_haddhsub_ps128(opcode, lhs.low, rhs.low);
    result.high = apply_avx_haddhsub_ps128(opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_haddhsub_pd128(uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    __m128d lhs_vec = sse2_math_pd_xmm_to_m128d(lhs);
    __m128d rhs_vec = sse2_math_pd_xmm_to_m128d(rhs);
    return sse2_math_pd_m128d_to_xmm(opcode == 0x7C ? _mm_hadd_pd(lhs_vec, rhs_vec)
                                                    : _mm_hsub_pd(lhs_vec, rhs_vec));
}

static AVXRegister256 apply_avx_haddhsub_pd256(uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_haddhsub_pd128(opcode, lhs.low, rhs.low);
    result.high = apply_avx_haddhsub_pd128(opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_dpps128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    float sum = 0.0f;
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        if ((imm8 & (1U << (lane + 4))) != 0) {
            float lhs_lane = sse_bits_to_float(get_xmm_lane_bits(lhs, lane));
            float rhs_lane = sse_bits_to_float(get_xmm_lane_bits(rhs, lane));
            sum += lhs_lane * rhs_lane;
        }
    }
    uint32_t sum_bits = sse_float_to_bits(sum);
    for (int lane = 0; lane < 4; lane++) {
        set_xmm_lane_bits(&result, lane, (imm8 & (1U << lane)) != 0 ? sum_bits : 0U);
    }
    return result;
}

static XMMRegister apply_avx_dppd128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    double sum = 0.0;
    XMMRegister result = {};
    if ((imm8 & 0x10) != 0) {
        sum += sse2_arith_pd_bits_to_double(lhs.low) * sse2_arith_pd_bits_to_double(rhs.low);
    }
    if ((imm8 & 0x20) != 0) {
        sum += sse2_arith_pd_bits_to_double(lhs.high) * sse2_arith_pd_bits_to_double(rhs.high);
    }
    uint64_t sum_bits = sse2_arith_pd_double_to_bits(sum);
    result.low = (imm8 & 0x01) != 0 ? sum_bits : 0ULL;
    result.high = (imm8 & 0x02) != 0 ? sum_bits : 0ULL;
    return result;
}

static unsigned int avx_host_rounding_mode(uint32_t mxcsr) {
    switch ((mxcsr >> 13) & 0x3U) {
    case 0: return _MM_ROUND_NEAREST;
    case 1: return _MM_ROUND_DOWN;
    case 2: return _MM_ROUND_UP;
    case 3: return _MM_ROUND_TOWARD_ZERO;
    default: return _MM_ROUND_TOWARD_ZERO;
    }
}

static __m128 apply_avx_round_ps_intrinsic(__m128 value, uint8_t imm8, uint32_t mxcsr) {
    if ((imm8 & 0x04) != 0) {
        unsigned int old_mode = _MM_GET_ROUNDING_MODE();
        _MM_SET_ROUNDING_MODE(avx_host_rounding_mode(mxcsr));
        __m128 result = _mm_round_ps(value, _MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC);
        _MM_SET_ROUNDING_MODE(old_mode);
        return result;
    }
    switch (imm8 & 0x03) {
    case 0: return _mm_round_ps(value, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    case 1: return _mm_round_ps(value, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
    case 2: return _mm_round_ps(value, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
    case 3: return _mm_round_ps(value, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    default:
        CPUEAXH_UNREACHABLE();
        return _mm_round_ps(value, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    }
}

static __m128d apply_avx_round_pd_intrinsic(__m128d value, uint8_t imm8, uint32_t mxcsr) {
    if ((imm8 & 0x04) != 0) {
        unsigned int old_mode = _MM_GET_ROUNDING_MODE();
        _MM_SET_ROUNDING_MODE(avx_host_rounding_mode(mxcsr));
        __m128d result = _mm_round_pd(value, _MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC);
        _MM_SET_ROUNDING_MODE(old_mode);
        return result;
    }
    switch (imm8 & 0x03) {
    case 0: return _mm_round_pd(value, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    case 1: return _mm_round_pd(value, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
    case 2: return _mm_round_pd(value, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
    case 3: return _mm_round_pd(value, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    default:
        CPUEAXH_UNREACHABLE();
        return _mm_round_pd(value, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    }
}

static __m128 apply_avx_round_ss_intrinsic(__m128 src1, __m128 rhs, uint8_t imm8, uint32_t mxcsr) {
    if ((imm8 & 0x04) != 0) {
        unsigned int old_mode = _MM_GET_ROUNDING_MODE();
        _MM_SET_ROUNDING_MODE(avx_host_rounding_mode(mxcsr));
        __m128 result = _mm_round_ss(src1, rhs, _MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC);
        _MM_SET_ROUNDING_MODE(old_mode);
        return result;
    }
    switch (imm8 & 0x03) {
    case 0: return _mm_round_ss(src1, rhs, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    case 1: return _mm_round_ss(src1, rhs, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
    case 2: return _mm_round_ss(src1, rhs, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
    case 3: return _mm_round_ss(src1, rhs, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    default:
        CPUEAXH_UNREACHABLE();
        return _mm_round_ss(src1, rhs, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    }
}

static __m128d apply_avx_round_sd_intrinsic(__m128d src1, __m128d rhs, uint8_t imm8, uint32_t mxcsr) {
    if ((imm8 & 0x04) != 0) {
        unsigned int old_mode = _MM_GET_ROUNDING_MODE();
        _MM_SET_ROUNDING_MODE(avx_host_rounding_mode(mxcsr));
        __m128d result = _mm_round_sd(src1, rhs, _MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC);
        _MM_SET_ROUNDING_MODE(old_mode);
        return result;
    }
    switch (imm8 & 0x03) {
    case 0: return _mm_round_sd(src1, rhs, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    case 1: return _mm_round_sd(src1, rhs, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
    case 2: return _mm_round_sd(src1, rhs, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC);
    case 3: return _mm_round_sd(src1, rhs, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    default:
        CPUEAXH_UNREACHABLE();
        return _mm_round_sd(src1, rhs, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    }
}

static XMMRegister apply_avx_round_ps128(XMMRegister rhs, uint8_t imm8, uint32_t mxcsr) {
    return sse_math_m128_to_xmm(apply_avx_round_ps_intrinsic(sse_math_xmm_to_m128(rhs), imm8, mxcsr));
}

static AVXRegister256 apply_avx_round_ps256(AVXRegister256 rhs, uint8_t imm8, uint32_t mxcsr) {
    AVXRegister256 result = {};
    result.low = apply_avx_round_ps128(rhs.low, imm8, mxcsr);
    result.high = apply_avx_round_ps128(rhs.high, imm8, mxcsr);
    return result;
}

static XMMRegister apply_avx_round_pd128(XMMRegister rhs, uint8_t imm8, uint32_t mxcsr) {
    return sse2_math_pd_m128d_to_xmm(apply_avx_round_pd_intrinsic(sse2_math_pd_xmm_to_m128d(rhs), imm8, mxcsr));
}

static AVXRegister256 apply_avx_round_pd256(AVXRegister256 rhs, uint8_t imm8, uint32_t mxcsr) {
    AVXRegister256 result = {};
    result.low = apply_avx_round_pd128(rhs.low, imm8, mxcsr);
    result.high = apply_avx_round_pd128(rhs.high, imm8, mxcsr);
    return result;
}

static XMMRegister apply_avx_round_ss128(XMMRegister src1, uint32_t rhs_bits, uint8_t imm8, uint32_t mxcsr) {
    XMMRegister rhs_scalar = {};
    set_xmm_lane_bits(&rhs_scalar, 0, rhs_bits);
    return sse_math_m128_to_xmm(apply_avx_round_ss_intrinsic(sse_math_xmm_to_m128(src1), sse_math_xmm_to_m128(rhs_scalar), imm8, mxcsr));
}

static XMMRegister apply_avx_round_sd128(XMMRegister src1, uint64_t rhs_bits, uint8_t imm8, uint32_t mxcsr) {
    XMMRegister rhs_scalar = {};
    rhs_scalar.low = rhs_bits;
    return sse2_math_pd_m128d_to_xmm(apply_avx_round_sd_intrinsic(sse2_math_pd_xmm_to_m128d(src1), sse2_math_pd_xmm_to_m128d(rhs_scalar), imm8, mxcsr));
}

static XMMRegister apply_avx_blend_ps128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        bool use_rhs = (imm8 & (1U << lane)) != 0;
        set_xmm_lane_bits(&result, lane, use_rhs ? get_xmm_lane_bits(rhs, lane) : get_xmm_lane_bits(lhs, lane));
    }
    return result;
}

static AVXRegister256 apply_avx_blend_ps256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t imm8) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        bool use_rhs = (imm8 & (1U << lane)) != 0;
        set_ymm_lane_bits(&result, lane, use_rhs ? get_ymm_lane_bits(rhs, lane) : get_ymm_lane_bits(lhs, lane));
    }
    return result;
}

static XMMRegister apply_avx_blend_pd128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    XMMRegister result = {};
    result.low = (imm8 & 0x01) != 0 ? rhs.low : lhs.low;
    result.high = (imm8 & 0x02) != 0 ? rhs.high : lhs.high;
    return result;
}

static AVXRegister256 apply_avx_blend_pd256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t imm8) {
    AVXRegister256 result = {};
    result.low.low = (imm8 & 0x01) != 0 ? rhs.low.low : lhs.low.low;
    result.low.high = (imm8 & 0x02) != 0 ? rhs.low.high : lhs.low.high;
    result.high.low = (imm8 & 0x04) != 0 ? rhs.high.low : lhs.high.low;
    result.high.high = (imm8 & 0x08) != 0 ? rhs.high.high : lhs.high.high;
    return result;
}

static uint16_t get_xmm_word(XMMRegister value, int index) {
    if (index < 4) {
        return (uint16_t)((value.low >> (index * 16)) & 0xFFFFU);
    }
    return (uint16_t)((value.high >> ((index - 4) * 16)) & 0xFFFFU);
}

static void set_xmm_word(XMMRegister* value, int index, uint16_t word) {
    uint64_t mask = 0xFFFFULL;
    if (index < 4) {
        uint64_t shift = (uint64_t)index * 16;
        value->low = (value->low & ~(mask << shift)) | ((uint64_t)word << shift);
    }
    else {
        uint64_t shift = (uint64_t)(index - 4) * 16;
        value->high = (value->high & ~(mask << shift)) | ((uint64_t)word << shift);
    }
}

static uint8_t get_xmm_byte(XMMRegister value, int index) {
    if (index < 8) {
        return (uint8_t)((value.low >> (index * 8)) & 0xFFU);
    }
    return (uint8_t)((value.high >> ((index - 8) * 8)) & 0xFFU);
}

static void set_xmm_byte(XMMRegister* value, int index, uint8_t byte_value) {
    uint64_t mask = 0xFFULL;
    if (index < 8) {
        uint64_t shift = (uint64_t)index * 8;
        value->low = (value->low & ~(mask << shift)) | ((uint64_t)byte_value << shift);
    }
    else {
        uint64_t shift = (uint64_t)(index - 8) * 8;
        value->high = (value->high & ~(mask << shift)) | ((uint64_t)byte_value << shift);
    }
}

static uint16_t get_ymm_word(AVXRegister256 value, int index) {
    return index < 8 ? get_xmm_word(value.low, index) : get_xmm_word(value.high, index - 8);
}

static void set_ymm_word(AVXRegister256* value, int index, uint16_t word) {
    if (index < 8) {
        set_xmm_word(&value->low, index, word);
    }
    else {
        set_xmm_word(&value->high, index - 8, word);
    }
}

static uint8_t get_ymm_byte(AVXRegister256 value, int index) {
    return index < 16 ? get_xmm_byte(value.low, index) : get_xmm_byte(value.high, index - 16);
}

static void set_ymm_byte(AVXRegister256* value, int index, uint8_t byte_value) {
    if (index < 16) {
        set_xmm_byte(&value->low, index, byte_value);
    }
    else {
        set_xmm_byte(&value->high, index - 16, byte_value);
    }
}

static XMMRegister apply_avx2_blendw128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    XMMRegister result = {};
    for (int lane = 0; lane < 8; lane++) {
        bool use_rhs = (imm8 & (1U << lane)) != 0;
        set_xmm_word(&result, lane, use_rhs ? get_xmm_word(rhs, lane) : get_xmm_word(lhs, lane));
    }
    return result;
}

static AVXRegister256 apply_avx2_blendw256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t imm8) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 16; lane++) {
        bool use_rhs = (imm8 & (1U << (lane & 0x07))) != 0;
        set_ymm_word(&result, lane, use_rhs ? get_ymm_word(rhs, lane) : get_ymm_word(lhs, lane));
    }
    return result;
}

static XMMRegister apply_avx2_blendvb128(XMMRegister lhs, XMMRegister rhs, XMMRegister mask) {
    XMMRegister result = {};
    for (int byte_index = 0; byte_index < 16; byte_index++) {
        bool use_rhs = (get_xmm_byte(mask, byte_index) & 0x80U) != 0;
        set_xmm_byte(&result, byte_index, use_rhs ? get_xmm_byte(rhs, byte_index) : get_xmm_byte(lhs, byte_index));
    }
    return result;
}

static AVXRegister256 apply_avx2_blendvb256(AVXRegister256 lhs, AVXRegister256 rhs, AVXRegister256 mask) {
    AVXRegister256 result = {};
    for (int byte_index = 0; byte_index < 32; byte_index++) {
        bool use_rhs = (get_ymm_byte(mask, byte_index) & 0x80U) != 0;
        set_ymm_byte(&result, byte_index, use_rhs ? get_ymm_byte(rhs, byte_index) : get_ymm_byte(lhs, byte_index));
    }
    return result;
}

static XMMRegister apply_avx_blendv_ps128(XMMRegister lhs, XMMRegister rhs, XMMRegister mask) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        bool use_rhs = (get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0;
        set_xmm_lane_bits(&result, lane, use_rhs ? get_xmm_lane_bits(rhs, lane) : get_xmm_lane_bits(lhs, lane));
    }
    return result;
}

static AVXRegister256 apply_avx_blendv_ps256(AVXRegister256 lhs, AVXRegister256 rhs, AVXRegister256 mask) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        bool use_rhs = (get_ymm_lane_bits(mask, lane) & 0x80000000U) != 0;
        set_ymm_lane_bits(&result, lane, use_rhs ? get_ymm_lane_bits(rhs, lane) : get_ymm_lane_bits(lhs, lane));
    }
    return result;
}

static XMMRegister apply_avx_blendv_pd128(XMMRegister lhs, XMMRegister rhs, XMMRegister mask) {
    XMMRegister result = {};
    result.low = (mask.low & 0x8000000000000000ULL) != 0 ? rhs.low : lhs.low;
    result.high = (mask.high & 0x8000000000000000ULL) != 0 ? rhs.high : lhs.high;
    return result;
}

static AVXRegister256 apply_avx_blendv_pd256(AVXRegister256 lhs, AVXRegister256 rhs, AVXRegister256 mask) {
    AVXRegister256 result = {};
    result.low.low = (mask.low.low & 0x8000000000000000ULL) != 0 ? rhs.low.low : lhs.low.low;
    result.low.high = (mask.low.high & 0x8000000000000000ULL) != 0 ? rhs.low.high : lhs.low.high;
    result.high.low = (mask.high.low & 0x8000000000000000ULL) != 0 ? rhs.high.low : lhs.high.low;
    result.high.high = (mask.high.high & 0x8000000000000000ULL) != 0 ? rhs.high.high : lhs.high.high;
    return result;
}

static XMMRegister apply_avx_int_logic128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    switch (opcode) {
    case 0xDB:
        result.low = lhs.low & rhs.low;
        result.high = lhs.high & rhs.high;
        break;
    case 0xDF:
        result.low = (~lhs.low) & rhs.low;
        result.high = (~lhs.high) & rhs.high;
        break;
    case 0xEB:
        result.low = lhs.low | rhs.low;
        result.high = lhs.high | rhs.high;
        break;
    case 0xEF:
        result.low = lhs.low ^ rhs.low;
        result.high = lhs.high ^ rhs.high;
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }
    return result;
}

static AVXRegister256 apply_avx_int_logic256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_int_logic128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx_int_logic128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_permilps_imm128(XMMRegister source, uint8_t imm8) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        int selector = (imm8 >> (lane * 2)) & 0x03;
        set_xmm_lane_bits(&result, lane, get_xmm_lane_bits(source, selector));
    }
    return result;
}

static AVXRegister256 apply_avx_permilps_imm256(AVXRegister256 source, uint8_t imm8) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        int lane_in_half = lane & 0x03;
        int half_base = lane < 4 ? 0 : 4;
        int selector = (imm8 >> (lane_in_half * 2)) & 0x03;
        set_ymm_lane_bits(&result, lane, get_ymm_lane_bits(source, half_base + selector));
    }
    return result;
}

static XMMRegister apply_avx_permilpd_imm128(XMMRegister source, uint8_t imm8) {
    XMMRegister result = {};
    result.low = ((imm8 & 0x01) != 0) ? source.high : source.low;
    result.high = ((imm8 & 0x02) != 0) ? source.high : source.low;
    return result;
}

static AVXRegister256 apply_avx_permilpd_imm256(AVXRegister256 source, uint8_t imm8) {
    AVXRegister256 result = {};
    result.low.low = ((imm8 & 0x01) != 0) ? source.low.high : source.low.low;
    result.low.high = ((imm8 & 0x02) != 0) ? source.low.high : source.low.low;
    result.high.low = ((imm8 & 0x04) != 0) ? source.high.high : source.high.low;
    result.high.high = ((imm8 & 0x08) != 0) ? source.high.high : source.high.low;
    return result;
}

static XMMRegister apply_avx_permilps_var128(XMMRegister source, XMMRegister control) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        int selector = (int)(get_xmm_lane_bits(control, lane) & 0x03U);
        set_xmm_lane_bits(&result, lane, get_xmm_lane_bits(source, selector));
    }
    return result;
}

static AVXRegister256 apply_avx_permilps_var256(AVXRegister256 source, AVXRegister256 control) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        int lane_in_half = lane & 0x03;
        int half_base = lane < 4 ? 0 : 4;
        int selector = (int)(get_ymm_lane_bits(control, lane) & 0x03U);
        set_ymm_lane_bits(&result, lane, get_ymm_lane_bits(source, half_base + selector));
    }
    return result;
}

static XMMRegister apply_avx_permilpd_var128(XMMRegister source, XMMRegister control) {
    XMMRegister result = {};
    result.low = (control.low & 0x01ULL) != 0 ? source.high : source.low;
    result.high = (control.high & 0x01ULL) != 0 ? source.high : source.low;
    return result;
}

static AVXRegister256 apply_avx_permilpd_var256(AVXRegister256 source, AVXRegister256 control) {
    AVXRegister256 result = {};
    result.low.low = (control.low.low & 0x01ULL) != 0 ? source.low.high : source.low.low;
    result.low.high = (control.low.high & 0x01ULL) != 0 ? source.low.high : source.low.low;
    result.high.low = (control.high.low & 0x01ULL) != 0 ? source.high.high : source.high.low;
    result.high.high = (control.high.high & 0x01ULL) != 0 ? source.high.high : source.high.low;
    return result;
}

static XMMRegister apply_avx2_pshufb128(XMMRegister source, XMMRegister control) {
    uint8_t source_bytes[16] = {};
    uint8_t control_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(source, source_bytes);
    sse2_pack_xmm_to_bytes(control, control_bytes);
    for (int index = 0; index < 16; index++) {
        uint8_t selector = control_bytes[index];
        result_bytes[index] = (selector & 0x80U) ? 0x00U : source_bytes[selector & 0x0FU];
    }
    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx2_pshufb256(AVXRegister256 source, AVXRegister256 control) {
    AVXRegister256 result = {};
    result.low = apply_avx2_pshufb128(source.low, control.low);
    result.high = apply_avx2_pshufb128(source.high, control.high);
    return result;
}

static XMMRegister apply_avx2_palignr128(XMMRegister src1, XMMRegister src2, uint8_t imm8) {
    uint8_t src1_bytes[16] = {};
    uint8_t src2_bytes[16] = {};
    uint8_t concat_bytes[32] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(src1, src1_bytes);
    sse2_pack_xmm_to_bytes(src2, src2_bytes);
    CPUEAXH_MEMCPY(concat_bytes, src2_bytes, 16);
    CPUEAXH_MEMCPY(concat_bytes + 16, src1_bytes, 16);
    for (int index = 0; index < 16; index++) {
        int source_index = (int)imm8 + index;
        result_bytes[index] = (source_index >= 0 && source_index < 32) ? concat_bytes[source_index] : 0x00U;
    }
    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx2_palignr256(AVXRegister256 src1, AVXRegister256 src2, uint8_t imm8) {
    AVXRegister256 result = {};
    result.low = apply_avx2_palignr128(src1.low, src2.low, imm8);
    result.high = apply_avx2_palignr128(src1.high, src2.high, imm8);
    return result;
}

static AVXRegister256 apply_avx2_perm32x8(AVXRegister256 data, AVXRegister256 control) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        uint32_t selector = get_ymm_lane_bits(control, lane) & 0x07U;
        set_ymm_lane_bits(&result, lane, get_ymm_lane_bits(data, (int)selector));
    }
    return result;
}

static AVXRegister256 apply_avx2_permq256(AVXRegister256 data, uint8_t imm8) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 4; lane++) {
        int selector = (imm8 >> (lane * 2)) & 0x03;
        set_ymm_pd_lane_bits(&result, lane, get_ymm_pd_lane_bits(data, selector));
    }
    return result;
}

static AVXRegister256 apply_avx_perm2f128_256(AVXRegister256 src1, AVXRegister256 src2, uint8_t imm8) {
    auto select_lane = [&](int selector) {
        switch (selector & 0x03) {
        case 0x00: return src1.low;
        case 0x01: return src1.high;
        case 0x02: return src2.low;
        default:   return src2.high;
        }
    };

    AVXRegister256 result = {};
    result.low = (imm8 & 0x08) != 0 ? XMMRegister{} : select_lane(imm8 & 0x03);
    result.high = (imm8 & 0x80) != 0 ? XMMRegister{} : select_lane((imm8 >> 4) & 0x03);
    return result;
}

static AVXRegister256 apply_avx_insertf128_256(AVXRegister256 src1, XMMRegister insert_value, uint8_t imm8) {
    AVXRegister256 result = src1;
    if ((imm8 & 0x01) != 0) {
        result.high = insert_value;
    }
    else {
        result.low = insert_value;
    }
    return result;
}

static XMMRegister apply_avx_extractf128_256(AVXRegister256 source, uint8_t imm8) {
    return (imm8 & 0x01) != 0 ? source.high : source.low;
}

static XMMRegister apply_avx_insertps_reg128(XMMRegister src1, XMMRegister src2, uint8_t imm8) {
    XMMRegister result = src1;
    int source_lane = (imm8 >> 6) & 0x03;
    int dest_lane = (imm8 >> 4) & 0x03;
    set_xmm_lane_bits(&result, dest_lane, get_xmm_lane_bits(src2, source_lane));
    for (int lane = 0; lane < 4; lane++) {
        if ((imm8 & (1U << lane)) != 0) {
            set_xmm_lane_bits(&result, lane, 0);
        }
    }
    return result;
}

static XMMRegister apply_avx_insertps_mem128(XMMRegister src1, uint32_t source_bits, uint8_t imm8) {
    XMMRegister result = src1;
    int dest_lane = (imm8 >> 4) & 0x03;
    set_xmm_lane_bits(&result, dest_lane, source_bits);
    for (int lane = 0; lane < 4; lane++) {
        if ((imm8 & (1U << lane)) != 0) {
            set_xmm_lane_bits(&result, lane, 0);
        }
    }
    return result;
}

static XMMRegister apply_avx_pinsrb128(XMMRegister src1, uint8_t source_value, uint8_t imm8) {
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(src1, result_bytes);
    result_bytes[imm8 & 0x0F] = source_value;
    return sse2_pack_bytes_to_xmm(result_bytes);
}

static XMMRegister apply_avx_pinsrw128(XMMRegister src1, uint16_t source_value, uint8_t imm8) {
    uint8_t result_bytes[16] = {};
    int dest_byte = (int)(imm8 & 0x07) * 2;
    sse2_pack_xmm_to_bytes(src1, result_bytes);
    result_bytes[dest_byte] = (uint8_t)(source_value & 0xFFU);
    result_bytes[dest_byte + 1] = (uint8_t)((source_value >> 8) & 0xFFU);
    return sse2_pack_bytes_to_xmm(result_bytes);
}

static XMMRegister apply_avx_pinsrd128(XMMRegister src1, uint32_t source_value, uint8_t imm8) {
    XMMRegister result = src1;
    set_xmm_lane_bits(&result, imm8 & 0x03, source_value);
    return result;
}

static XMMRegister apply_avx_pinsrq128(XMMRegister src1, uint64_t source_value, uint8_t imm8) {
    XMMRegister result = src1;
    if ((imm8 & 0x01) != 0) {
        result.high = source_value;
    }
    else {
        result.low = source_value;
    }
    return result;
}

static XMMRegister apply_avx_phminposuw128(XMMRegister source) {
    uint8_t source_bytes[16] = {};
    uint16_t minimum = 0;
    uint16_t minimum_index = 0;
    sse2_pack_xmm_to_bytes(source, source_bytes);

    minimum = (uint16_t)source_bytes[0] | (uint16_t)((uint16_t)source_bytes[1] << 8);
    for (uint16_t index = 1; index < 8; ++index) {
        int byte_offset = index * 2;
        uint16_t value = (uint16_t)source_bytes[byte_offset]
                       | (uint16_t)((uint16_t)source_bytes[byte_offset + 1] << 8);
        if (value < minimum) {
            minimum = value;
            minimum_index = index;
        }
    }

    XMMRegister result = {};
    result.low = (uint64_t)minimum | ((uint64_t)minimum_index << 16);
    return result;
}

static AVXRegister256 broadcast_avx_ss256(uint32_t value) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        set_ymm_lane_bits(&result, lane, value);
    }
    return result;
}

static AVXRegister256 broadcast_avx_sd256(uint64_t value) {
    AVXRegister256 result = {};
    result.low.low = value;
    result.low.high = value;
    result.high.low = value;
    result.high.high = value;
    return result;
}

static AVXRegister256 broadcast_avx_f128_256(XMMRegister value) {
    AVXRegister256 result = {};
    result.low = value;
    result.high = value;
    return result;
}

static AVXRegister256 broadcast_avx2_byte256(uint8_t value) {
    AVXRegister256 result = {};
    uint64_t pattern = 0x0101010101010101ULL * (uint64_t)value;
    result.low.low = pattern;
    result.low.high = pattern;
    result.high.low = pattern;
    result.high.high = pattern;
    return result;
}

static AVXRegister256 broadcast_avx2_word256(uint16_t value) {
    AVXRegister256 result = {};
    uint64_t pattern = (uint64_t)value;
    pattern |= pattern << 16;
    pattern |= pattern << 32;
    result.low.low = pattern;
    result.low.high = pattern;
    result.high.low = pattern;
    result.high.high = pattern;
    return result;
}

static AVXRegister256 broadcast_avx2_dword256(uint32_t value) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        set_ymm_lane_bits(&result, lane, value);
    }
    return result;
}

static AVXRegister256 broadcast_avx2_qword256(uint64_t value) {
    AVXRegister256 result = {};
    result.low.low = value;
    result.low.high = value;
    result.high.low = value;
    result.high.high = value;
    return result;
}

static XMMRegister apply_avx_maskmov_load_ps128(XMMRegister mask, XMMRegister source) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        if ((get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0) {
            set_xmm_lane_bits(&result, lane, get_xmm_lane_bits(source, lane));
        }
    }
    return result;
}

static AVXRegister256 apply_avx_maskmov_load_ps256(AVXRegister256 mask, AVXRegister256 source) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        if ((get_ymm_lane_bits(mask, lane) & 0x80000000U) != 0) {
            set_ymm_lane_bits(&result, lane, get_ymm_lane_bits(source, lane));
        }
    }
    return result;
}

static XMMRegister apply_avx_maskmov_load_pd128(XMMRegister mask, XMMRegister source) {
    XMMRegister result = {};
    result.low = (mask.low & 0x8000000000000000ULL) != 0 ? source.low : 0;
    result.high = (mask.high & 0x8000000000000000ULL) != 0 ? source.high : 0;
    return result;
}

static AVXRegister256 apply_avx_maskmov_load_pd256(AVXRegister256 mask, AVXRegister256 source) {
    AVXRegister256 result = {};
    result.low.low = (mask.low.low & 0x8000000000000000ULL) != 0 ? source.low.low : 0;
    result.low.high = (mask.low.high & 0x8000000000000000ULL) != 0 ? source.low.high : 0;
    result.high.low = (mask.high.low & 0x8000000000000000ULL) != 0 ? source.high.low : 0;
    result.high.high = (mask.high.high & 0x8000000000000000ULL) != 0 ? source.high.high : 0;
    return result;
}

static void avx_store_u32_le(uint8_t* bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)(value & 0xFFU);
    bytes[offset + 1] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[offset + 2] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[offset + 3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void avx_write_masked_dwords(CPU_CONTEXT* ctx, uint64_t address, const uint32_t* values, const bool* element_mask, int lanes) {
    uint8_t bytes[32] = {};
    uint64_t element_values[8] = {};
    uint8_t* byte_ptrs[32] = {};

    for (int lane = 0; lane < lanes; ++lane) {
        element_values[lane] = values[lane];
        avx_store_u32_le(bytes, (size_t)(lane * 4), values[lane]);
    }

    if (!cpu_resolve_masked_write_element_ptrs(ctx, address, element_mask, element_values, (size_t)lanes, 4, byte_ptrs)) {
        return;
    }

    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        for (int lane = 0; lane < lanes; ++lane) {
            if (element_mask[lane]) {
                cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + (uint64_t)(lane * 4), 4, values[lane]);
            }
        }
    }

    cpu_commit_masked_write_elements(bytes, element_mask, (size_t)lanes, 4, byte_ptrs);
}

static void avx_write_masked_qwords(CPU_CONTEXT* ctx, uint64_t address, const uint64_t* values, const bool* element_mask, int lanes) {
    uint8_t bytes[32] = {};
    uint8_t* byte_ptrs[32] = {};

    for (int lane = 0; lane < lanes; ++lane) {
        cpu_store_u64_le(bytes + (lane * 8), values[lane]);
    }

    if (!cpu_resolve_masked_write_element_ptrs(ctx, address, element_mask, values, (size_t)lanes, 8, byte_ptrs)) {
        return;
    }

    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        for (int lane = 0; lane < lanes; ++lane) {
            if (element_mask[lane]) {
                cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + (uint64_t)(lane * 8), 8, values[lane]);
            }
        }
    }

    cpu_commit_masked_write_elements(bytes, element_mask, (size_t)lanes, 8, byte_ptrs);
}

static void execute_avx_maskmov_store_ps128(CPU_CONTEXT* ctx, uint64_t address, XMMRegister mask, XMMRegister source) {
    uint32_t values[4] = {};
    bool element_mask[4] = {};
    for (int lane = 0; lane < 4; lane++) {
        values[lane] = get_xmm_lane_bits(source, lane);
        element_mask[lane] = (get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0;
    }
    avx_write_masked_dwords(ctx, address, values, element_mask, 4);
}

static void execute_avx_maskmov_store_ps256(CPU_CONTEXT* ctx, uint64_t address, AVXRegister256 mask, AVXRegister256 source) {
    uint32_t values[8] = {};
    bool element_mask[8] = {};
    for (int lane = 0; lane < 8; lane++) {
        values[lane] = get_ymm_lane_bits(source, lane);
        element_mask[lane] = (get_ymm_lane_bits(mask, lane) & 0x80000000U) != 0;
    }
    avx_write_masked_dwords(ctx, address, values, element_mask, 8);
}

static void execute_avx_maskmov_store_pd128(CPU_CONTEXT* ctx, uint64_t address, XMMRegister mask, XMMRegister source) {
    uint64_t values[2] = { source.low, source.high };
    bool element_mask[2] = {
        (mask.low & 0x8000000000000000ULL) != 0,
        (mask.high & 0x8000000000000000ULL) != 0
    };
    avx_write_masked_qwords(ctx, address, values, element_mask, 2);
}

static void execute_avx_maskmov_store_pd256(CPU_CONTEXT* ctx, uint64_t address, AVXRegister256 mask, AVXRegister256 source) {
    uint64_t values[4] = { source.low.low, source.low.high, source.high.low, source.high.high };
    bool element_mask[4] = {
        (mask.low.low & 0x8000000000000000ULL) != 0,
        (mask.low.high & 0x8000000000000000ULL) != 0,
        (mask.high.low & 0x8000000000000000ULL) != 0,
        (mask.high.high & 0x8000000000000000ULL) != 0
    };
    avx_write_masked_qwords(ctx, address, values, element_mask, 4);
}

static XMMRegister apply_avx2_pmaskmov_load128(XMMRegister mask, XMMRegister source, bool is_qword) {
    XMMRegister result = {};
    if (is_qword) {
        result.low = (mask.low & 0x8000000000000000ULL) != 0 ? source.low : 0;
        result.high = (mask.high & 0x8000000000000000ULL) != 0 ? source.high : 0;
        return result;
    }

    for (int lane = 0; lane < 4; lane++) {
        if ((get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0) {
            set_xmm_lane_bits(&result, lane, get_xmm_lane_bits(source, lane));
        }
    }
    return result;
}

static AVXRegister256 apply_avx2_pmaskmov_load256(AVXRegister256 mask, AVXRegister256 source, bool is_qword) {
    AVXRegister256 result = {};
    if (is_qword) {
        for (int lane = 0; lane < 4; lane++) {
            if ((get_ymm_pd_lane_bits(mask, lane) & 0x8000000000000000ULL) != 0) {
                set_ymm_pd_lane_bits(&result, lane, get_ymm_pd_lane_bits(source, lane));
            }
        }
        return result;
    }

    for (int lane = 0; lane < 8; lane++) {
        if ((get_ymm_lane_bits(mask, lane) & 0x80000000U) != 0) {
            set_ymm_lane_bits(&result, lane, get_ymm_lane_bits(source, lane));
        }
    }
    return result;
}

static void execute_avx2_pmaskmov_store128(CPU_CONTEXT* ctx, uint64_t address, XMMRegister mask, XMMRegister source, bool is_qword) {
    if (is_qword) {
        uint64_t values[2] = { source.low, source.high };
        bool element_mask[2] = {
            (mask.low & 0x8000000000000000ULL) != 0,
            (mask.high & 0x8000000000000000ULL) != 0
        };
        avx_write_masked_qwords(ctx, address, values, element_mask, 2);
        return;
    }

    uint32_t values[4] = {};
    bool element_mask[4] = {};
    for (int lane = 0; lane < 4; lane++) {
        values[lane] = get_xmm_lane_bits(source, lane);
        element_mask[lane] = (get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0;
    }
    avx_write_masked_dwords(ctx, address, values, element_mask, 4);
}

static void execute_avx2_pmaskmov_store256(CPU_CONTEXT* ctx, uint64_t address, AVXRegister256 mask, AVXRegister256 source, bool is_qword) {
    if (is_qword) {
        uint64_t values[4] = {};
        bool element_mask[4] = {};
        for (int lane = 0; lane < 4; lane++) {
            values[lane] = get_ymm_pd_lane_bits(source, lane);
            element_mask[lane] = (get_ymm_pd_lane_bits(mask, lane) & 0x8000000000000000ULL) != 0;
        }
        avx_write_masked_qwords(ctx, address, values, element_mask, 4);
        return;
    }

    uint32_t values[8] = {};
    bool element_mask[8] = {};
    for (int lane = 0; lane < 8; lane++) {
        values[lane] = get_ymm_lane_bits(source, lane);
        element_mask[lane] = (get_ymm_lane_bits(mask, lane) & 0x80000000U) != 0;
    }
    avx_write_masked_dwords(ctx, address, values, element_mask, 8);
}

static void set_avx_test_flags(CPU_CONTEXT* ctx, bool zf, bool cf) {
    set_flag(ctx, RFLAGS_ZF, zf);
    set_flag(ctx, RFLAGS_CF, cf);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, false);
    set_flag(ctx, RFLAGS_AF, false);
    set_flag(ctx, RFLAGS_PF, false);
}

static bool compute_avx_test_zf128(XMMRegister lhs, XMMRegister rhs) {
    return ((lhs.low & rhs.low) == 0) && ((lhs.high & rhs.high) == 0);
}

static bool compute_avx_test_cf128(XMMRegister lhs, XMMRegister rhs) {
    return ((((~lhs.low) & rhs.low) == 0) && (((~lhs.high) & rhs.high) == 0));
}

static bool compute_avx_test_zf256(AVXRegister256 lhs, AVXRegister256 rhs) {
    return compute_avx_test_zf128(lhs.low, rhs.low) && compute_avx_test_zf128(lhs.high, rhs.high);
}

static bool compute_avx_test_cf256(AVXRegister256 lhs, AVXRegister256 rhs) {
    return compute_avx_test_cf128(lhs.low, rhs.low) && compute_avx_test_cf128(lhs.high, rhs.high);
}

static XMMRegister apply_avx_int_cmp128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_int_cmp_xmm_to_bytes(lhs, lhs_bytes);
    sse2_int_cmp_xmm_to_bytes(rhs, rhs_bytes);

    switch (opcode) {
    case 0x74:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = (lhs_bytes[index] == rhs_bytes[index]) ? 0xFFU : 0x00U;
        }
        break;
    case 0x75:
        for (int index = 0; index < 8; index++) {
            sse2_int_cmp_write_u16(result_bytes, index, sse2_int_cmp_read_u16(lhs_bytes, index) == sse2_int_cmp_read_u16(rhs_bytes, index) ? 0xFFFFU : 0x0000U);
        }
        break;
    case 0x76:
        for (int index = 0; index < 4; index++) {
            sse2_int_cmp_write_u32(result_bytes, index, sse2_int_cmp_read_u32(lhs_bytes, index) == sse2_int_cmp_read_u32(rhs_bytes, index) ? 0xFFFFFFFFU : 0x00000000U);
        }
        break;
    case 0x29:
        for (int index = 0; index < 2; index++) {
            sse2_int_arith_write_u64(result_bytes, index, sse2_int_arith_read_u64(lhs_bytes, index) == sse2_int_arith_read_u64(rhs_bytes, index) ? 0xFFFFFFFFFFFFFFFFULL : 0ULL);
        }
        break;
    case 0x37:
        for (int index = 0; index < 2; index++) {
            int64_t lhs_qword = (int64_t)sse2_int_arith_read_u64(lhs_bytes, index);
            int64_t rhs_qword = (int64_t)sse2_int_arith_read_u64(rhs_bytes, index);
            sse2_int_arith_write_u64(result_bytes, index, lhs_qword > rhs_qword ? 0xFFFFFFFFFFFFFFFFULL : 0ULL);
        }
        break;
    case 0x64:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = ((int8_t)lhs_bytes[index] > (int8_t)rhs_bytes[index]) ? 0xFFU : 0x00U;
        }
        break;
    case 0x65:
        for (int index = 0; index < 8; index++) {
            sse2_int_cmp_write_u16(result_bytes, index, sse2_int_cmp_read_i16(lhs_bytes, index) > sse2_int_cmp_read_i16(rhs_bytes, index) ? 0xFFFFU : 0x0000U);
        }
        break;
    case 0x66:
        for (int index = 0; index < 4; index++) {
            sse2_int_cmp_write_u32(result_bytes, index, sse2_int_cmp_read_i32(lhs_bytes, index) > sse2_int_cmp_read_i32(rhs_bytes, index) ? 0xFFFFFFFFU : 0x00000000U);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_int_cmp_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx_int_cmp256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_int_cmp128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx_int_cmp128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static uint16_t avx_pack_saturate_i32_to_u16(int32_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > 65535) {
        return 65535;
    }
    return (uint16_t)value;
}

static XMMRegister apply_avx_pack128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(lhs, lhs_bytes);
    sse2_pack_xmm_to_bytes(rhs, rhs_bytes);

    switch (opcode) {
    case 0x60:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 1, false, result_bytes);
        break;
    case 0x61:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 2, false, result_bytes);
        break;
    case 0x62:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 4, false, result_bytes);
        break;
    case 0x63:
        for (int index = 0; index < 8; index++) {
            result_bytes[index] = (uint8_t)sse2_pack_saturate_i16_to_i8(sse2_pack_read_i16(lhs_bytes, index));
            result_bytes[8 + index] = (uint8_t)sse2_pack_saturate_i16_to_i8(sse2_pack_read_i16(rhs_bytes, index));
        }
        break;
    case 0x67:
        for (int index = 0; index < 8; index++) {
            result_bytes[index] = sse2_pack_saturate_i16_to_u8(sse2_pack_read_i16(lhs_bytes, index));
            result_bytes[8 + index] = sse2_pack_saturate_i16_to_u8(sse2_pack_read_i16(rhs_bytes, index));
        }
        break;
    case 0x68:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 1, true, result_bytes);
        break;
    case 0x69:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 2, true, result_bytes);
        break;
    case 0x6A:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 4, true, result_bytes);
        break;
    case 0x6B:
        for (int index = 0; index < 4; index++) {
            sse2_pack_write_i16(result_bytes, index, sse2_pack_saturate_i32_to_i16(sse2_pack_read_i32(lhs_bytes, index)));
            sse2_pack_write_i16(result_bytes, 4 + index, sse2_pack_saturate_i32_to_i16(sse2_pack_read_i32(rhs_bytes, index)));
        }
        break;
    case 0x2B:
        for (int index = 0; index < 4; index++) {
            sse2_int_arith_write_u16(result_bytes, index, avx_pack_saturate_i32_to_u16(sse2_pack_read_i32(lhs_bytes, index)));
            sse2_int_arith_write_u16(result_bytes, 4 + index, avx_pack_saturate_i32_to_u16(sse2_pack_read_i32(rhs_bytes, index)));
        }
        break;
    case 0x6C:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 8, false, result_bytes);
        break;
    case 0x6D:
        sse2_pack_interleave(lhs_bytes, rhs_bytes, 8, true, result_bytes);
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx_pack256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_pack128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx_pack128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_pshufd128(XMMRegister src, uint8_t imm8) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        set_sse_shuffle_lane_bits(&result, lane, get_sse_shuffle_lane_bits(src, (imm8 >> (lane * 2)) & 0x03));
    }
    return result;
}

static AVXRegister256 apply_avx_pshufd256(AVXRegister256 src, uint8_t imm8) {
    AVXRegister256 result = {};
    result.low = apply_avx_pshufd128(src.low, imm8);
    result.high = apply_avx_pshufd128(src.high, imm8);
    return result;
}

static AVXRegister256 apply_avx_pshufhw256(AVXRegister256 src, uint8_t imm8) {
    AVXRegister256 result = {};
    result.low = apply_sse2_pshufhw128(src.low, imm8);
    result.high = apply_sse2_pshufhw128(src.high, imm8);
    return result;
}

static AVXRegister256 apply_avx_pshuflw256(AVXRegister256 src, uint8_t imm8) {
    AVXRegister256 result = {};
    result.low = apply_sse2_pshuflw128(src.low, imm8);
    result.high = apply_sse2_pshuflw128(src.high, imm8);
    return result;
}

static XMMRegister apply_avx_int_minmax128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(lhs, lhs_bytes);
    sse2_pack_xmm_to_bytes(rhs, rhs_bytes);

    switch (opcode) {
    case 0xDA:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = (lhs_bytes[index] < rhs_bytes[index]) ? lhs_bytes[index] : rhs_bytes[index];
        }
        break;
    case 0xDE:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = (lhs_bytes[index] > rhs_bytes[index]) ? lhs_bytes[index] : rhs_bytes[index];
        }
        break;
    case 0xEA:
        for (int index = 0; index < 8; index++) {
            int16_t lhs_word = sse2_pack_read_i16(lhs_bytes, index);
            int16_t rhs_word = sse2_pack_read_i16(rhs_bytes, index);
            sse2_pack_write_i16(result_bytes, index, lhs_word < rhs_word ? lhs_word : rhs_word);
        }
        break;
    case 0xEE:
        for (int index = 0; index < 8; index++) {
            int16_t lhs_word = sse2_pack_read_i16(lhs_bytes, index);
            int16_t rhs_word = sse2_pack_read_i16(rhs_bytes, index);
            sse2_pack_write_i16(result_bytes, index, lhs_word > rhs_word ? lhs_word : rhs_word);
        }
        break;
    case 0x38:
        for (int index = 0; index < 16; index++) {
            int8_t lhs_byte = (int8_t)lhs_bytes[index];
            int8_t rhs_byte = (int8_t)rhs_bytes[index];
            result_bytes[index] = (uint8_t)(lhs_byte < rhs_byte ? lhs_byte : rhs_byte);
        }
        break;
    case 0x39:
        for (int index = 0; index < 4; index++) {
            int32_t lhs_dword = sse2_pack_read_i32(lhs_bytes, index);
            int32_t rhs_dword = sse2_pack_read_i32(rhs_bytes, index);
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)(lhs_dword < rhs_dword ? lhs_dword : rhs_dword));
        }
        break;
    case 0x3A:
        for (int index = 0; index < 8; index++) {
            uint16_t lhs_word = sse2_int_arith_read_u16(lhs_bytes, index);
            uint16_t rhs_word = sse2_int_arith_read_u16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, lhs_word < rhs_word ? lhs_word : rhs_word);
        }
        break;
    case 0x3B:
        for (int index = 0; index < 4; index++) {
            uint32_t lhs_dword = sse2_int_arith_read_u32(lhs_bytes, index);
            uint32_t rhs_dword = sse2_int_arith_read_u32(rhs_bytes, index);
            sse2_int_arith_write_u32(result_bytes, index, lhs_dword < rhs_dword ? lhs_dword : rhs_dword);
        }
        break;
    case 0x3C:
        for (int index = 0; index < 16; index++) {
            int8_t lhs_byte = (int8_t)lhs_bytes[index];
            int8_t rhs_byte = (int8_t)rhs_bytes[index];
            result_bytes[index] = (uint8_t)(lhs_byte > rhs_byte ? lhs_byte : rhs_byte);
        }
        break;
    case 0x3D:
        for (int index = 0; index < 4; index++) {
            int32_t lhs_dword = sse2_pack_read_i32(lhs_bytes, index);
            int32_t rhs_dword = sse2_pack_read_i32(rhs_bytes, index);
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)(lhs_dword > rhs_dword ? lhs_dword : rhs_dword));
        }
        break;
    case 0x3E:
        for (int index = 0; index < 8; index++) {
            uint16_t lhs_word = sse2_int_arith_read_u16(lhs_bytes, index);
            uint16_t rhs_word = sse2_int_arith_read_u16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, lhs_word > rhs_word ? lhs_word : rhs_word);
        }
        break;
    case 0x3F:
        for (int index = 0; index < 4; index++) {
            uint32_t lhs_dword = sse2_int_arith_read_u32(lhs_bytes, index);
            uint32_t rhs_dword = sse2_int_arith_read_u32(rhs_bytes, index);
            sse2_int_arith_write_u32(result_bytes, index, lhs_dword > rhs_dword ? lhs_dword : rhs_dword);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx_int_minmax256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_int_minmax128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx_int_minmax128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx2_sign128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(lhs, lhs_bytes);
    sse2_pack_xmm_to_bytes(rhs, rhs_bytes);

    switch (opcode) {
    case 0x08:
        for (int index = 0; index < 16; index++) {
            int8_t value = (int8_t)lhs_bytes[index];
            int8_t sign = (int8_t)rhs_bytes[index];
            int8_t result = sign == 0 ? 0 : (sign < 0 ? (int8_t)(-value) : value);
            result_bytes[index] = (uint8_t)result;
        }
        break;
    case 0x09:
        for (int index = 0; index < 8; index++) {
            int16_t value = sse2_int_arith_read_i16(lhs_bytes, index);
            int16_t sign = sse2_int_arith_read_i16(rhs_bytes, index);
            int16_t result = sign == 0 ? 0 : (sign < 0 ? (int16_t)(-value) : value);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)result);
        }
        break;
    case 0x0A:
        for (int index = 0; index < 4; index++) {
            int32_t value = sse2_pack_read_i32(lhs_bytes, index);
            int32_t sign = sse2_pack_read_i32(rhs_bytes, index);
            int32_t result = sign == 0 ? 0 : (sign < 0 ? -value : value);
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)result);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx2_sign256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx2_sign128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx2_sign128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx2_abs128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister value) {
    uint8_t src_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(value, src_bytes);

    switch (opcode) {
    case 0x1C:
        for (int index = 0; index < 16; index++) {
            int8_t element = (int8_t)src_bytes[index];
            result_bytes[index] = (uint8_t)(element < 0 ? (int8_t)(-element) : element);
        }
        break;
    case 0x1D:
        for (int index = 0; index < 8; index++) {
            int16_t element = sse2_int_arith_read_i16(src_bytes, index);
            int16_t result = element < 0 ? (int16_t)(-element) : element;
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)result);
        }
        break;
    case 0x1E:
        for (int index = 0; index < 4; index++) {
            int32_t element = sse2_pack_read_i32(src_bytes, index);
            int32_t result = element < 0 ? -element : element;
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)result);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx2_abs256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 value) {
    AVXRegister256 result = {};
    result.low = apply_avx2_abs128(ctx, opcode, value.low);
    result.high = apply_avx2_abs128(ctx, opcode, value.high);
    return result;
}

static int avx2_pmov_source_size(CPU_CONTEXT* ctx, uint8_t opcode, bool is_256) {
    switch (opcode & 0x0F) {
    case 0x00:
    case 0x03:
    case 0x05:
        return is_256 ? 16 : 8;
    case 0x01:
    case 0x04:
        return is_256 ? 8 : 4;
    case 0x02:
        return is_256 ? 4 : 2;
    default:
        raise_ud_ctx(ctx);
        return 0;
    }
}

static XMMRegister read_avx2_pmov_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint8_t opcode, bool is_256) {
    XMMRegister source = {};
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 0x03) {
        return get_xmm128(ctx, avx_vex_rm_index(ctx, inst->modrm));
    }

    switch (avx2_pmov_source_size(ctx, opcode, is_256)) {
    case 2:
        source.low = read_memory_word(ctx, inst->mem_address);
        break;
    case 4:
        source.low = read_memory_dword(ctx, inst->mem_address);
        break;
    case 8:
        source.low = read_memory_qword(ctx, inst->mem_address);
        break;
    case 16:
        source = read_xmm_memory(ctx, inst->mem_address);
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }
    return source;
}

static int avx_vsib_index_register(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (!inst->has_sib) {
        raise_ud_ctx(ctx);
    }

    int index = (inst->sib >> 3) & 0x07;
    if (ctx->rex_x && inst->address_size == 64) {
        index |= 0x08;
    }
    return index;
}

static uint64_t avx_vsib_base_address(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (!inst->has_sib) {
        raise_ud_ctx(ctx);
    }

    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 0x03) {
        raise_ud_ctx(ctx);
    }

    uint8_t raw_base = inst->sib & 0x07;
    bool no_base = mod == 0x00 && raw_base == 0x05;
    uint64_t address = 0;

    if (no_base) {
        if (inst->address_size == 64) {
            address = (uint64_t)(int64_t)inst->displacement;
        }
        else {
            address = (uint32_t)inst->displacement;
        }
    }
    else {
        int base = raw_base;
        if (ctx->rex_b && inst->address_size == 64) {
            base |= 0x08;
        }
        address = ctx->regs[base];
        if (inst->address_size == 32) {
            address = (uint32_t)address;
        }
    }

    if (mod == 0x01) {
        address += (int8_t)(inst->displacement & 0xFF);
    }
    else if (mod == 0x02) {
        address += (int32_t)inst->displacement;
    }

    if (inst->address_size == 32) {
        address &= 0xFFFFFFFFULL;
    }
    return address;
}

static uint64_t avx_vsib_element_address(uint64_t base_address, uint8_t sib, int64_t index, int address_size) {
    int64_t scale = 1LL << ((sib >> 6) & 0x03);
    uint64_t address = base_address + (uint64_t)(index * scale);
    if (address_size == 32) {
        address &= 0xFFFFFFFFULL;
    }
    return address;
}

static void validate_avx_gather_operands(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 0x03 || !inst->has_sib) {
        raise_ud_ctx(ctx);
    }

    int dest = avx_vex_dest_index(ctx, inst->modrm);
    int mask = avx_vex_source1_index(prefix);
    int index = avx_vsib_index_register(ctx, inst);
    if (dest == mask || dest == index || mask == index) {
        raise_ud_ctx(ctx);
    }
}

static void execute_avx_gather_dps(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix, const DecodedInstruction* inst, bool is_256) {
    validate_avx_gather_operands(ctx, prefix, inst);

    int dest = avx_vex_dest_index(ctx, inst->modrm);
    int mask_reg = avx_vex_source1_index(prefix);
    int index_reg = avx_vsib_index_register(ctx, inst);
    uint64_t base_address = avx_vsib_base_address(ctx, inst);

    if (is_256) {
        AVXRegister256 result = get_ymm256(ctx, dest);
        AVXRegister256 mask = get_ymm256(ctx, mask_reg);
        AVXRegister256 indices = get_ymm256(ctx, index_reg);
        for (int lane = 0; lane < 8; lane++) {
            if ((get_ymm_lane_bits(mask, lane) & 0x80000000U) != 0) {
                uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)(int32_t)get_ymm_lane_bits(indices, lane), inst->address_size);
                set_ymm_lane_bits(&result, lane, read_memory_dword(ctx, address));
            }
        }
        set_ymm256(ctx, dest, result);
        set_ymm256(ctx, mask_reg, {});
        return;
    }

    XMMRegister result = get_xmm128(ctx, dest);
    XMMRegister mask = get_xmm128(ctx, mask_reg);
    XMMRegister indices = get_xmm128(ctx, index_reg);
    for (int lane = 0; lane < 4; lane++) {
        if ((get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0) {
            uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)(int32_t)get_xmm_lane_bits(indices, lane), inst->address_size);
            set_xmm_lane_bits(&result, lane, read_memory_dword(ctx, address));
        }
    }
    set_xmm128(ctx, dest, result);
    clear_ymm_upper128(ctx, dest);
    set_xmm128(ctx, mask_reg, {});
    clear_ymm_upper128(ctx, mask_reg);
}

static void execute_avx_gather_dpd(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix, const DecodedInstruction* inst, bool is_256) {
    validate_avx_gather_operands(ctx, prefix, inst);

    int dest = avx_vex_dest_index(ctx, inst->modrm);
    int mask_reg = avx_vex_source1_index(prefix);
    int index_reg = avx_vsib_index_register(ctx, inst);
    uint64_t base_address = avx_vsib_base_address(ctx, inst);

    if (is_256) {
        AVXRegister256 result = get_ymm256(ctx, dest);
        AVXRegister256 mask = get_ymm256(ctx, mask_reg);
        XMMRegister indices = get_xmm128(ctx, index_reg);
        for (int lane = 0; lane < 4; lane++) {
            if ((get_ymm_pd_lane_bits(mask, lane) >> 63) != 0) {
                uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)(int32_t)get_xmm_lane_bits(indices, lane), inst->address_size);
                set_ymm_pd_lane_bits(&result, lane, read_memory_qword(ctx, address));
            }
        }
        set_ymm256(ctx, dest, result);
        set_ymm256(ctx, mask_reg, {});
        return;
    }

    XMMRegister result = get_xmm128(ctx, dest);
    XMMRegister mask = get_xmm128(ctx, mask_reg);
    XMMRegister indices = get_xmm128(ctx, index_reg);
    for (int lane = 0; lane < 2; lane++) {
        if ((get_sse2_arith_pd_lane_bits(mask, lane) >> 63) != 0) {
            uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)(int32_t)get_xmm_lane_bits(indices, lane), inst->address_size);
            set_sse2_arith_pd_lane_bits(&result, lane, read_memory_qword(ctx, address));
        }
    }
    set_xmm128(ctx, dest, result);
    clear_ymm_upper128(ctx, dest);
    set_xmm128(ctx, mask_reg, {});
    clear_ymm_upper128(ctx, mask_reg);
}

static void execute_avx_gather_qps(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix, const DecodedInstruction* inst, bool is_256) {
    validate_avx_gather_operands(ctx, prefix, inst);

    int dest = avx_vex_dest_index(ctx, inst->modrm);
    int mask_reg = avx_vex_source1_index(prefix);
    int index_reg = avx_vsib_index_register(ctx, inst);
    uint64_t base_address = avx_vsib_base_address(ctx, inst);

    XMMRegister result = get_xmm128(ctx, dest);
    XMMRegister mask = get_xmm128(ctx, mask_reg);
    int active_lanes = is_256 ? 4 : 2;

    if (!is_256) {
        set_xmm_lane_bits(&result, 2, 0);
        set_xmm_lane_bits(&result, 3, 0);
    }

    if (is_256) {
        AVXRegister256 indices = get_ymm256(ctx, index_reg);
        for (int lane = 0; lane < active_lanes; lane++) {
            if ((get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0) {
                uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)get_ymm_pd_lane_bits(indices, lane), inst->address_size);
                set_xmm_lane_bits(&result, lane, read_memory_dword(ctx, address));
            }
        }
    }
    else {
        XMMRegister indices = get_xmm128(ctx, index_reg);
        for (int lane = 0; lane < active_lanes; lane++) {
            if ((get_xmm_lane_bits(mask, lane) & 0x80000000U) != 0) {
                uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)get_sse2_arith_pd_lane_bits(indices, lane), inst->address_size);
                set_xmm_lane_bits(&result, lane, read_memory_dword(ctx, address));
            }
        }
    }

    set_xmm128(ctx, dest, result);
    clear_ymm_upper128(ctx, dest);
    set_xmm128(ctx, mask_reg, {});
    clear_ymm_upper128(ctx, mask_reg);
}

static void execute_avx_gather_qpd(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix, const DecodedInstruction* inst, bool is_256) {
    validate_avx_gather_operands(ctx, prefix, inst);

    int dest = avx_vex_dest_index(ctx, inst->modrm);
    int mask_reg = avx_vex_source1_index(prefix);
    int index_reg = avx_vsib_index_register(ctx, inst);
    uint64_t base_address = avx_vsib_base_address(ctx, inst);

    if (is_256) {
        AVXRegister256 result = get_ymm256(ctx, dest);
        AVXRegister256 mask = get_ymm256(ctx, mask_reg);
        AVXRegister256 indices = get_ymm256(ctx, index_reg);
        for (int lane = 0; lane < 4; lane++) {
            if ((get_ymm_pd_lane_bits(mask, lane) >> 63) != 0) {
                uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)get_ymm_pd_lane_bits(indices, lane), inst->address_size);
                set_ymm_pd_lane_bits(&result, lane, read_memory_qword(ctx, address));
            }
        }
        set_ymm256(ctx, dest, result);
        set_ymm256(ctx, mask_reg, {});
        return;
    }

    XMMRegister result = get_xmm128(ctx, dest);
    XMMRegister mask = get_xmm128(ctx, mask_reg);
    XMMRegister indices = get_xmm128(ctx, index_reg);
    for (int lane = 0; lane < 2; lane++) {
        if ((get_sse2_arith_pd_lane_bits(mask, lane) >> 63) != 0) {
            uint64_t address = avx_vsib_element_address(base_address, inst->sib, (int64_t)get_sse2_arith_pd_lane_bits(indices, lane), inst->address_size);
            set_sse2_arith_pd_lane_bits(&result, lane, read_memory_qword(ctx, address));
        }
    }
    set_xmm128(ctx, dest, result);
    clear_ymm_upper128(ctx, dest);
    set_xmm128(ctx, mask_reg, {});
    clear_ymm_upper128(ctx, mask_reg);
}

static void avx_write_u16_32(uint8_t bytes[32], int index, uint16_t value) {
    int offset = index * 2;
    bytes[offset] = (uint8_t)(value & 0xFFU);
    bytes[offset + 1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void avx_write_u32_32(uint8_t bytes[32], int index, uint32_t value) {
    int offset = index * 4;
    for (int byte_index = 0; byte_index < 4; byte_index++) {
        bytes[offset + byte_index] = (uint8_t)((value >> (byte_index * 8)) & 0xFFU);
    }
}

static void avx_write_u64_32(uint8_t bytes[32], int index, uint64_t value) {
    int offset = index * 8;
    for (int byte_index = 0; byte_index < 8; byte_index++) {
        bytes[offset + byte_index] = (uint8_t)((value >> (byte_index * 8)) & 0xFFU);
    }
}

static XMMRegister apply_avx2_pmovx128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister source) {
    uint8_t src_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    bool zero_extend = opcode >= 0x30;
    sse2_pack_xmm_to_bytes(source, src_bytes);

    switch (opcode) {
    case 0x20:
    case 0x30:
        for (int index = 0; index < 8; index++) {
            int16_t value = zero_extend ? (int16_t)src_bytes[index] : (int16_t)(int8_t)src_bytes[index];
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)value);
        }
        break;
    case 0x21:
    case 0x31:
        for (int index = 0; index < 4; index++) {
            int32_t value = zero_extend ? (int32_t)src_bytes[index] : (int32_t)(int8_t)src_bytes[index];
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)value);
        }
        break;
    case 0x22:
    case 0x32:
        for (int index = 0; index < 2; index++) {
            int64_t value = zero_extend ? (int64_t)src_bytes[index] : (int64_t)(int8_t)src_bytes[index];
            sse2_int_arith_write_u64(result_bytes, index, (uint64_t)value);
        }
        break;
    case 0x23:
    case 0x33:
        for (int index = 0; index < 4; index++) {
            int32_t value = zero_extend ? (int32_t)sse2_int_arith_read_u16(src_bytes, index) : (int32_t)sse2_int_arith_read_i16(src_bytes, index);
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)value);
        }
        break;
    case 0x24:
    case 0x34:
        for (int index = 0; index < 2; index++) {
            int64_t value = zero_extend ? (int64_t)sse2_int_arith_read_u16(src_bytes, index) : (int64_t)sse2_int_arith_read_i16(src_bytes, index);
            sse2_int_arith_write_u64(result_bytes, index, (uint64_t)value);
        }
        break;
    case 0x25:
    case 0x35:
        for (int index = 0; index < 2; index++) {
            int64_t value = zero_extend ? (int64_t)sse2_int_arith_read_u32(src_bytes, index) : (int64_t)(int32_t)sse2_int_arith_read_u32(src_bytes, index);
            sse2_int_arith_write_u64(result_bytes, index, (uint64_t)value);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx2_pmovx256(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister source) {
    uint8_t src_bytes[16] = {};
    uint8_t result_bytes[32] = {};
    bool zero_extend = opcode >= 0x30;
    sse2_pack_xmm_to_bytes(source, src_bytes);

    switch (opcode) {
    case 0x20:
    case 0x30:
        for (int index = 0; index < 16; index++) {
            int16_t value = zero_extend ? (int16_t)src_bytes[index] : (int16_t)(int8_t)src_bytes[index];
            avx_write_u16_32(result_bytes, index, (uint16_t)value);
        }
        break;
    case 0x21:
    case 0x31:
        for (int index = 0; index < 8; index++) {
            int32_t value = zero_extend ? (int32_t)src_bytes[index] : (int32_t)(int8_t)src_bytes[index];
            avx_write_u32_32(result_bytes, index, (uint32_t)value);
        }
        break;
    case 0x22:
    case 0x32:
        for (int index = 0; index < 4; index++) {
            int64_t value = zero_extend ? (int64_t)src_bytes[index] : (int64_t)(int8_t)src_bytes[index];
            avx_write_u64_32(result_bytes, index, (uint64_t)value);
        }
        break;
    case 0x23:
    case 0x33:
        for (int index = 0; index < 8; index++) {
            int32_t value = zero_extend ? (int32_t)sse2_int_arith_read_u16(src_bytes, index) : (int32_t)sse2_int_arith_read_i16(src_bytes, index);
            avx_write_u32_32(result_bytes, index, (uint32_t)value);
        }
        break;
    case 0x24:
    case 0x34:
        for (int index = 0; index < 4; index++) {
            int64_t value = zero_extend ? (int64_t)sse2_int_arith_read_u16(src_bytes, index) : (int64_t)sse2_int_arith_read_i16(src_bytes, index);
            avx_write_u64_32(result_bytes, index, (uint64_t)value);
        }
        break;
    case 0x25:
    case 0x35:
        for (int index = 0; index < 4; index++) {
            int64_t value = zero_extend ? (int64_t)sse2_int_arith_read_u32(src_bytes, index) : (int64_t)(int32_t)sse2_int_arith_read_u32(src_bytes, index);
            avx_write_u64_32(result_bytes, index, (uint64_t)value);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    AVXRegister256 result = {};
    result.low = sse2_pack_bytes_to_xmm(result_bytes);
    result.high = sse2_pack_bytes_to_xmm(result_bytes + 16);
    return result;
}

static XMMRegister apply_avx2_horizontal128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(lhs, lhs_bytes);
    sse2_pack_xmm_to_bytes(rhs, rhs_bytes);

    switch (opcode) {
    case 0x01:
    case 0x05:
    case 0x03:
    case 0x07:
        for (int block = 0; block < 2; block++) {
            const uint8_t* source_bytes = block == 0 ? lhs_bytes : rhs_bytes;
            for (int index = 0; index < 4; index++) {
                int16_t first = sse2_pack_read_i16(source_bytes, index * 2);
                int16_t second = sse2_pack_read_i16(source_bytes, index * 2 + 1);
                int32_t value = (opcode == 0x05 || opcode == 0x07)
                    ? (int32_t)first - (int32_t)second
                    : (int32_t)first + (int32_t)second;
                if (opcode == 0x03 || opcode == 0x07) {
                    sse2_pack_write_i16(result_bytes, block * 4 + index, sse2_pack_saturate_i32_to_i16(value));
                }
                else {
                    sse2_pack_write_i16(result_bytes, block * 4 + index, (int16_t)value);
                }
            }
        }
        break;
    case 0x02:
    case 0x06:
        for (int block = 0; block < 2; block++) {
            const uint8_t* source_bytes = block == 0 ? lhs_bytes : rhs_bytes;
            for (int index = 0; index < 2; index++) {
                int32_t first = sse2_pack_read_i32(source_bytes, index * 2);
                int32_t second = sse2_pack_read_i32(source_bytes, index * 2 + 1);
                uint32_t value = (uint32_t)((opcode == 0x06) ? (first - second) : (first + second));
                sse2_int_arith_write_u32(result_bytes, block * 2 + index, value);
            }
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_pack_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx2_horizontal256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx2_horizontal128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx2_horizontal128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_int_arith128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_int_arith_xmm_to_bytes(lhs, lhs_bytes);
    sse2_int_arith_xmm_to_bytes(rhs, rhs_bytes);

    switch (opcode) {
    case 0xFC:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = (uint8_t)(lhs_bytes[index] + rhs_bytes[index]);
        }
        break;
    case 0xFD:
        for (int index = 0; index < 8; index++) {
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)(sse2_int_arith_read_u16(lhs_bytes, index) + sse2_int_arith_read_u16(rhs_bytes, index)));
        }
        break;
    case 0xFE:
        for (int index = 0; index < 4; index++) {
            sse2_int_arith_write_u32(result_bytes, index, sse2_int_arith_read_u32(lhs_bytes, index) + sse2_int_arith_read_u32(rhs_bytes, index));
        }
        break;
    case 0xD4:
        for (int index = 0; index < 2; index++) {
            sse2_int_arith_write_u64(result_bytes, index, sse2_int_arith_read_u64(lhs_bytes, index) + sse2_int_arith_read_u64(rhs_bytes, index));
        }
        break;
    case 0xDC:
        for (int index = 0; index < 16; index++) {
            uint16_t sum = (uint16_t)lhs_bytes[index] + (uint16_t)rhs_bytes[index];
            result_bytes[index] = (sum > 0xFFU) ? 0xFFU : (uint8_t)sum;
        }
        break;
    case 0xDD:
        for (int index = 0; index < 8; index++) {
            uint32_t sum = (uint32_t)sse2_int_arith_read_u16(lhs_bytes, index) + (uint32_t)sse2_int_arith_read_u16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)(sum > 0xFFFFU ? 0xFFFFU : sum));
        }
        break;
    case 0xE0:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = sse2_int_arith_average_u8(lhs_bytes[index], rhs_bytes[index]);
        }
        break;
    case 0xE3:
        for (int index = 0; index < 8; index++) {
            sse2_int_arith_write_u16(result_bytes, index, sse2_int_arith_average_u16(sse2_int_arith_read_u16(lhs_bytes, index), sse2_int_arith_read_u16(rhs_bytes, index)));
        }
        break;
    case 0xEC:
        for (int index = 0; index < 16; index++) {
            int16_t sum = (int16_t)(int8_t)lhs_bytes[index] + (int16_t)(int8_t)rhs_bytes[index];
            result_bytes[index] = (uint8_t)sse2_int_arith_saturate_i16_to_i8(sum);
        }
        break;
    case 0xED:
        for (int index = 0; index < 8; index++) {
            int32_t sum = (int32_t)sse2_int_arith_read_i16(lhs_bytes, index) + (int32_t)sse2_int_arith_read_i16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)sse2_int_arith_saturate_i32_to_i16(sum));
        }
        break;
    case 0xF8:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = (uint8_t)(lhs_bytes[index] - rhs_bytes[index]);
        }
        break;
    case 0xF9:
        for (int index = 0; index < 8; index++) {
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)(sse2_int_arith_read_u16(lhs_bytes, index) - sse2_int_arith_read_u16(rhs_bytes, index)));
        }
        break;
    case 0xFA:
        for (int index = 0; index < 4; index++) {
            sse2_int_arith_write_u32(result_bytes, index, sse2_int_arith_read_u32(lhs_bytes, index) - sse2_int_arith_read_u32(rhs_bytes, index));
        }
        break;
    case 0xFB:
        for (int index = 0; index < 2; index++) {
            sse2_int_arith_write_u64(result_bytes, index, sse2_int_arith_read_u64(lhs_bytes, index) - sse2_int_arith_read_u64(rhs_bytes, index));
        }
        break;
    case 0xD8:
        for (int index = 0; index < 16; index++) {
            result_bytes[index] = (lhs_bytes[index] < rhs_bytes[index]) ? 0 : (uint8_t)(lhs_bytes[index] - rhs_bytes[index]);
        }
        break;
    case 0xD9:
        for (int index = 0; index < 8; index++) {
            uint16_t lhs = sse2_int_arith_read_u16(lhs_bytes, index);
            uint16_t rhs = sse2_int_arith_read_u16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, lhs < rhs ? 0 : (uint16_t)(lhs - rhs));
        }
        break;
    case 0xE8:
        for (int index = 0; index < 16; index++) {
            int16_t diff = (int16_t)(int8_t)lhs_bytes[index] - (int16_t)(int8_t)rhs_bytes[index];
            result_bytes[index] = (uint8_t)sse2_int_arith_saturate_i16_to_i8(diff);
        }
        break;
    case 0xE9:
        for (int index = 0; index < 8; index++) {
            int32_t diff = (int32_t)sse2_int_arith_read_i16(lhs_bytes, index) - (int32_t)sse2_int_arith_read_i16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)sse2_int_arith_saturate_i32_to_i16(diff));
        }
        break;
    case 0x04:
        for (int index = 0; index < 8; index++) {
            int base = index * 2;
            int32_t sum = (int32_t)((uint32_t)lhs_bytes[base] * (int32_t)(int8_t)rhs_bytes[base])
                        + (int32_t)((uint32_t)lhs_bytes[base + 1] * (int32_t)(int8_t)rhs_bytes[base + 1]);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)sse2_int_arith_saturate_i32_to_i16(sum));
        }
        break;
    case 0x0B:
        for (int index = 0; index < 8; index++) {
            int32_t product = (int32_t)sse2_int_arith_read_i16(lhs_bytes, index) * (int32_t)sse2_int_arith_read_i16(rhs_bytes, index);
            int32_t rounded = (product + 0x4000) >> 15;
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)sse2_int_arith_saturate_i32_to_i16(rounded));
        }
        break;
    case 0x28:
        for (int index = 0; index < 2; index++) {
            int lane = index * 2;
            int64_t product = (int64_t)(int32_t)sse2_int_arith_read_u32(lhs_bytes, lane) * (int64_t)(int32_t)sse2_int_arith_read_u32(rhs_bytes, lane);
            sse2_int_arith_write_u64(result_bytes, index, (uint64_t)product);
        }
        break;
    case 0x40:
        for (int index = 0; index < 4; index++) {
            int64_t product = (int64_t)(int32_t)sse2_int_arith_read_u32(lhs_bytes, index) * (int64_t)(int32_t)sse2_int_arith_read_u32(rhs_bytes, index);
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)product);
        }
        break;
    case 0xD5:
        for (int index = 0; index < 8; index++) {
            int32_t product = (int32_t)sse2_int_arith_read_i16(lhs_bytes, index) * (int32_t)sse2_int_arith_read_i16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)product);
        }
        break;
    case 0xE4:
        for (int index = 0; index < 8; index++) {
            uint32_t product = (uint32_t)sse2_int_arith_read_u16(lhs_bytes, index) * (uint32_t)sse2_int_arith_read_u16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)(product >> 16));
        }
        break;
    case 0xE5:
        for (int index = 0; index < 8; index++) {
            int32_t product = (int32_t)sse2_int_arith_read_i16(lhs_bytes, index) * (int32_t)sse2_int_arith_read_i16(rhs_bytes, index);
            sse2_int_arith_write_u16(result_bytes, index, (uint16_t)(product >> 16));
        }
        break;
    case 0xF6:
        for (int block = 0; block < 2; block++) {
            uint64_t sum = 0;
            for (int index = 0; index < 8; index++) {
                int offset = block * 8 + index;
                sum += (uint64_t)((lhs_bytes[offset] > rhs_bytes[offset]) ? (lhs_bytes[offset] - rhs_bytes[offset]) : (rhs_bytes[offset] - lhs_bytes[offset]));
            }
            sse2_int_arith_write_u64(result_bytes, block, sum);
        }
        break;
    case 0xF4: {
        uint64_t product0 = (uint64_t)sse2_int_arith_read_u32(lhs_bytes, 0) * (uint64_t)sse2_int_arith_read_u32(rhs_bytes, 0);
        uint64_t product1 = (uint64_t)sse2_int_arith_read_u32(lhs_bytes, 2) * (uint64_t)sse2_int_arith_read_u32(rhs_bytes, 2);
        sse2_int_arith_write_u64(result_bytes, 0, product0);
        sse2_int_arith_write_u64(result_bytes, 1, product1);
        break;
    }
    case 0xF5:
        for (int index = 0; index < 4; index++) {
            int base = index * 2;
            int64_t sum = (int64_t)sse2_int_arith_read_i16(lhs_bytes, base) * (int64_t)sse2_int_arith_read_i16(rhs_bytes, base)
                        + (int64_t)sse2_int_arith_read_i16(lhs_bytes, base + 1) * (int64_t)sse2_int_arith_read_i16(rhs_bytes, base + 1);
            sse2_int_arith_write_u32(result_bytes, index, (uint32_t)sum);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    return sse2_int_arith_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx_int_arith256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_int_arith128(ctx, opcode, lhs.low, rhs.low);
    result.high = apply_avx_int_arith128(ctx, opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_mpsadbw128(XMMRegister lhs, XMMRegister rhs, uint8_t imm8) {
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    int rhs_offset = (int)(imm8 & 0x03) * 4;
    int lhs_offset = (int)((imm8 >> 2) & 0x01) * 4;

    sse2_int_arith_xmm_to_bytes(lhs, lhs_bytes);
    sse2_int_arith_xmm_to_bytes(rhs, rhs_bytes);

    for (int index = 0; index < 8; index++) {
        uint16_t sum = 0;
        for (int element = 0; element < 4; element++) {
            int lhs_byte = lhs_bytes[lhs_offset + index + element];
            int rhs_byte = rhs_bytes[rhs_offset + element];
            sum = (uint16_t)(sum + (uint16_t)(lhs_byte > rhs_byte ? (lhs_byte - rhs_byte) : (rhs_byte - lhs_byte)));
        }
        sse2_int_arith_write_u16(result_bytes, index, sum);
    }

    return sse2_int_arith_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx_mpsadbw256(AVXRegister256 lhs, AVXRegister256 rhs, uint8_t imm8) {
    uint8_t lhs_bytes[32] = {};
    uint8_t rhs_bytes[32] = {};
    uint8_t result_bytes[32] = {};
    sse2_int_arith_xmm_to_bytes(lhs.low, lhs_bytes);
    sse2_int_arith_xmm_to_bytes(lhs.high, lhs_bytes + 16);
    sse2_int_arith_xmm_to_bytes(rhs.low, rhs_bytes);
    sse2_int_arith_xmm_to_bytes(rhs.high, rhs_bytes + 16);

    for (int lane = 0; lane < 2; lane++) {
        int lane_base = lane * 16;
        int rhs_offset = lane == 0
            ? (int)(imm8 & 0x03) * 4
            : 16 + (int)((imm8 >> 3) & 0x03) * 4;
        int lhs_offset = lane == 0
            ? (int)((imm8 >> 2) & 0x01) * 4
            : 16 + (int)((imm8 >> 5) & 0x01) * 4;

        for (int index = 0; index < 8; index++) {
            uint16_t sum = 0;
            for (int element = 0; element < 4; element++) {
                int lhs_byte = lhs_bytes[lhs_offset + index + element];
                int rhs_byte = rhs_bytes[rhs_offset + element];
                sum = (uint16_t)(sum + (uint16_t)(lhs_byte > rhs_byte ? (lhs_byte - rhs_byte) : (rhs_byte - lhs_byte)));
            }
            avx_write_u16_32(result_bytes, lane * 8 + index, sum);
        }
    }

    AVXRegister256 result = {};
    result.low = sse2_int_arith_bytes_to_xmm(result_bytes);
    result.high = sse2_int_arith_bytes_to_xmm(result_bytes + 16);
    return result;
}

static XMMRegister apply_avx_shift128(CPU_CONTEXT* ctx, uint8_t opcode, uint8_t group, XMMRegister src, uint64_t count) {
    uint8_t src_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_shift_xmm_to_bytes(src, src_bytes);

    if (opcode == 0x71 || opcode == 0x72 || opcode == 0x73) {
        if (opcode == 0x71) {
            if (group == 2) {
                sse2_shift_apply_psrlw(src_bytes, count, result_bytes);
            }
            else if (group == 4) {
                sse2_shift_apply_psraw(src_bytes, count, result_bytes);
            }
            else if (group == 6) {
                sse2_shift_apply_psllw(src_bytes, count, result_bytes);
            }
            else {
                raise_ud_ctx(ctx);
            }
        }
        else if (opcode == 0x72) {
            if (group == 2) {
                sse2_shift_apply_psrld(src_bytes, count, result_bytes);
            }
            else if (group == 4) {
                sse2_shift_apply_psrad(src_bytes, count, result_bytes);
            }
            else if (group == 6) {
                sse2_shift_apply_pslld(src_bytes, count, result_bytes);
            }
            else {
                raise_ud_ctx(ctx);
            }
        }
        else {
            if (group == 2) {
                sse2_shift_apply_psrlq(src_bytes, count, result_bytes);
            }
            else if (group == 3) {
                sse2_shift_apply_psrldq(src_bytes, count, result_bytes);
            }
            else if (group == 6) {
                sse2_shift_apply_psllq(src_bytes, count, result_bytes);
            }
            else if (group == 7) {
                sse2_shift_apply_pslldq(src_bytes, count, result_bytes);
            }
            else {
                raise_ud_ctx(ctx);
            }
        }
    }
    else {
        switch (opcode) {
        case 0xD1:
            sse2_shift_apply_psrlw(src_bytes, count, result_bytes);
            break;
        case 0xD2:
            sse2_shift_apply_psrld(src_bytes, count, result_bytes);
            break;
        case 0xD3:
            sse2_shift_apply_psrlq(src_bytes, count, result_bytes);
            break;
        case 0xE1:
            sse2_shift_apply_psraw(src_bytes, count, result_bytes);
            break;
        case 0xE2:
            sse2_shift_apply_psrad(src_bytes, count, result_bytes);
            break;
        case 0xF1:
            sse2_shift_apply_psllw(src_bytes, count, result_bytes);
            break;
        case 0xF2:
            sse2_shift_apply_pslld(src_bytes, count, result_bytes);
            break;
        case 0xF3:
            sse2_shift_apply_psllq(src_bytes, count, result_bytes);
            break;
        default:
            raise_ud_ctx(ctx);
            break;
        }
    }

    return sse2_shift_bytes_to_xmm(result_bytes);
}

static AVXRegister256 apply_avx_shift256(CPU_CONTEXT* ctx, uint8_t opcode, uint8_t group, AVXRegister256 src, uint64_t count) {
    AVXRegister256 result = {};
    result.low = apply_avx_shift128(ctx, opcode, group, src.low, count);
    result.high = apply_avx_shift128(ctx, opcode, group, src.high, count);
    return result;
}

static XMMRegister apply_avx2_shiftvar_dword128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister src, XMMRegister counts) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        uint32_t value = get_xmm_lane_bits(src, lane);
        uint32_t count = get_xmm_lane_bits(counts, lane) & 0x1FU;
        uint32_t shifted = 0;
        if (opcode == 0x47) {
            shifted = value << count;
        }
        else if (opcode == 0x45) {
            shifted = value >> count;
        }
        else if (opcode == 0x46) {
            shifted = (uint32_t)(((int32_t)value) >> count);
        }
        else {
            raise_ud_ctx(ctx);
        }
        set_xmm_lane_bits(&result, lane, shifted);
    }
    return result;
}

static AVXRegister256 apply_avx2_shiftvar_dword256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 src, AVXRegister256 counts) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        uint32_t value = get_ymm_lane_bits(src, lane);
        uint32_t count = get_ymm_lane_bits(counts, lane) & 0x1FU;
        uint32_t shifted = 0;
        if (opcode == 0x47) {
            shifted = value << count;
        }
        else if (opcode == 0x45) {
            shifted = value >> count;
        }
        else if (opcode == 0x46) {
            shifted = (uint32_t)(((int32_t)value) >> count);
        }
        else {
            raise_ud_ctx(ctx);
        }
        set_ymm_lane_bits(&result, lane, shifted);
    }
    return result;
}

static XMMRegister apply_avx2_shiftvar_qword128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister src, XMMRegister counts) {
    XMMRegister result = {};
    uint64_t low_count = counts.low & 0x3FULL;
    uint64_t high_count = counts.high & 0x3FULL;
    if (opcode == 0x47) {
        result.low = src.low << low_count;
        result.high = src.high << high_count;
    }
    else if (opcode == 0x45) {
        result.low = src.low >> low_count;
        result.high = src.high >> high_count;
    }
    else {
        raise_ud_ctx(ctx);
    }
    return result;
}

static AVXRegister256 apply_avx2_shiftvar_qword256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 src, AVXRegister256 counts) {
    AVXRegister256 result = {};
    uint64_t count0 = counts.low.low & 0x3FULL;
    uint64_t count1 = counts.low.high & 0x3FULL;
    uint64_t count2 = counts.high.low & 0x3FULL;
    uint64_t count3 = counts.high.high & 0x3FULL;
    if (opcode == 0x47) {
        result.low.low = src.low.low << count0;
        result.low.high = src.low.high << count1;
        result.high.low = src.high.low << count2;
        result.high.high = src.high.high << count3;
    }
    else if (opcode == 0x45) {
        result.low.low = src.low.low >> count0;
        result.low.high = src.low.high >> count1;
        result.high.low = src.high.low >> count2;
        result.high.high = src.high.high >> count3;
    }
    else {
        raise_ud_ctx(ctx);
    }
    return result;
}

static XMMRegister apply_avx_arith_pd128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    for (int lane = 0; lane < 2; lane++) {
        double lhs_lane = sse2_arith_pd_bits_to_double(get_sse2_arith_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_arith_pd_bits_to_double(get_sse2_arith_pd_lane_bits(rhs, lane));
        set_sse2_arith_pd_lane_bits(&result, lane, sse2_arith_pd_double_to_bits(apply_sse2_arith_pd_scalar(ctx, opcode, lhs_lane, rhs_lane)));
    }
    return result;
}

static AVXRegister256 apply_avx_arith_pd256(CPU_CONTEXT* ctx, uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 4; lane++) {
        double lhs_lane = sse2_arith_pd_bits_to_double(get_ymm_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_arith_pd_bits_to_double(get_ymm_pd_lane_bits(rhs, lane));
        set_ymm_pd_lane_bits(&result, lane, sse2_arith_pd_double_to_bits(apply_sse2_arith_pd_scalar(ctx, opcode, lhs_lane, rhs_lane)));
    }
    return result;
}

static XMMRegister apply_avx_addsub_pd128(XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    for (int lane = 0; lane < 2; lane++) {
        double lhs_lane = sse2_arith_pd_bits_to_double(get_sse2_arith_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_arith_pd_bits_to_double(get_sse2_arith_pd_lane_bits(rhs, lane));
        double value = (lane & 0x01) == 0 ? (lhs_lane - rhs_lane) : (lhs_lane + rhs_lane);
        set_sse2_arith_pd_lane_bits(&result, lane, sse2_arith_pd_double_to_bits(value));
    }
    return result;
}

static AVXRegister256 apply_avx_addsub_pd256(AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 4; lane++) {
        double lhs_lane = sse2_arith_pd_bits_to_double(get_ymm_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_arith_pd_bits_to_double(get_ymm_pd_lane_bits(rhs, lane));
        double value = (lane & 0x01) == 0 ? (lhs_lane - rhs_lane) : (lhs_lane + rhs_lane);
        set_ymm_pd_lane_bits(&result, lane, sse2_arith_pd_double_to_bits(value));
    }
    return result;
}

static XMMRegister apply_avx_addsub_ps128(XMMRegister lhs, XMMRegister rhs) {
    XMMRegister result = {};
    for (int lane = 0; lane < 4; lane++) {
        float lhs_lane = sse_bits_to_float(get_xmm_lane_bits(lhs, lane));
        float rhs_lane = sse_bits_to_float(get_xmm_lane_bits(rhs, lane));
        float value = (lane & 0x01) == 0 ? (lhs_lane - rhs_lane) : (lhs_lane + rhs_lane);
        set_xmm_lane_bits(&result, lane, sse_float_to_bits(value));
    }
    return result;
}

static AVXRegister256 apply_avx_addsub_ps256(AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane++) {
        float lhs_lane = sse_bits_to_float(get_ymm_lane_bits(lhs, lane));
        float rhs_lane = sse_bits_to_float(get_ymm_lane_bits(rhs, lane));
        float value = (lane & 0x01) == 0 ? (lhs_lane - rhs_lane) : (lhs_lane + rhs_lane);
        set_ymm_lane_bits(&result, lane, sse_float_to_bits(value));
    }
    return result;
}

static XMMRegister apply_avx_arith_sd128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister src1, uint64_t rhs_bits) {
    XMMRegister result = src1;
    double lhs_scalar = sse2_arith_pd_bits_to_double(src1.low);
    double rhs_scalar = sse2_arith_pd_bits_to_double(rhs_bits);
    result.low = sse2_arith_pd_double_to_bits(apply_sse2_arith_pd_scalar(ctx, opcode, lhs_scalar, rhs_scalar));
    return result;
}

static XMMRegister apply_avx_fmadd_sd128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister src1, XMMRegister src2, uint64_t src3_bits) {
    XMMRegister src3 = {};
    src3.low = src3_bits;
    unsigned int old_mode = _MM_GET_ROUNDING_MODE();
    _MM_SET_ROUNDING_MODE(avx_host_rounding_mode(ctx->mxcsr));
    // Mirror the Intel pseudocode operand ordering for 132/213/231 while keeping the scalar op fused.
    switch (opcode) {
    case 0x99: {
        XMMRegister result = sse2_math_pd_m128d_to_xmm(_mm_fmadd_sd(sse2_math_pd_xmm_to_m128d(src1),
                                                                     sse2_math_pd_xmm_to_m128d(src3),
                                                                     sse2_math_pd_xmm_to_m128d(src2)));
        _MM_SET_ROUNDING_MODE(old_mode);
        return result;
    }
    case 0xA9: {
        XMMRegister result = sse2_math_pd_m128d_to_xmm(_mm_fmadd_sd(sse2_math_pd_xmm_to_m128d(src1),
                                                                     sse2_math_pd_xmm_to_m128d(src2),
                                                                     sse2_math_pd_xmm_to_m128d(src3)));
        _MM_SET_ROUNDING_MODE(old_mode);
        return result;
    }
    case 0xB9: {
        XMMRegister result = sse2_math_pd_m128d_to_xmm(_mm_fmadd_sd(sse2_math_pd_xmm_to_m128d(src2),
                                                                     sse2_math_pd_xmm_to_m128d(src3),
                                                                     sse2_math_pd_xmm_to_m128d(src1)));
        _MM_SET_ROUNDING_MODE(old_mode);
        result.high = src1.high;
        return result;
    }
    default:
        _MM_SET_ROUNDING_MODE(old_mode);
        raise_ud_ctx(ctx);
        return src1;
    }
}

static XMMRegister apply_avx_movss_load128(XMMRegister src1, uint32_t rhs_bits) {
    XMMRegister result = src1;
    set_xmm_lane_bits(&result, 0, rhs_bits);
    return result;
}

static XMMRegister apply_avx_movlps_load128(XMMRegister src1, uint64_t rhs_bits) {
    XMMRegister result = src1;
    result.low = rhs_bits;
    return result;
}

static XMMRegister apply_avx_movhps_load128(XMMRegister src1, uint64_t rhs_bits) {
    XMMRegister result = src1;
    result.high = rhs_bits;
    return result;
}

static XMMRegister apply_avx_movhlps128(XMMRegister src1, XMMRegister src2) {
    XMMRegister result = src1;
    result.low = src2.high;
    return result;
}

static XMMRegister apply_avx_movlhps128(XMMRegister src1, XMMRegister src2) {
    XMMRegister result = src1;
    result.high = src2.low;
    return result;
}

static XMMRegister apply_avx_movddup128(XMMRegister source) {
    XMMRegister result = {};
    result.low = source.low;
    result.high = source.low;
    return result;
}

static XMMRegister apply_avx_movddup_load128(uint64_t source_bits) {
    XMMRegister result = {};
    result.low = source_bits;
    result.high = source_bits;
    return result;
}

static AVXRegister256 apply_avx_movddup256(AVXRegister256 source) {
    AVXRegister256 result = {};
    result.low.low = source.low.low;
    result.low.high = source.low.low;
    result.high.low = source.high.low;
    result.high.high = source.high.low;
    return result;
}

static XMMRegister apply_avx_movsldup128(XMMRegister source) {
    XMMRegister result = {};
    set_xmm_lane_bits(&result, 0, get_xmm_lane_bits(source, 0));
    set_xmm_lane_bits(&result, 1, get_xmm_lane_bits(source, 0));
    set_xmm_lane_bits(&result, 2, get_xmm_lane_bits(source, 2));
    set_xmm_lane_bits(&result, 3, get_xmm_lane_bits(source, 2));
    return result;
}

static AVXRegister256 apply_avx_movsldup256(AVXRegister256 source) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane += 2) {
        uint32_t value = get_ymm_lane_bits(source, lane);
        set_ymm_lane_bits(&result, lane, value);
        set_ymm_lane_bits(&result, lane + 1, value);
    }
    return result;
}

static XMMRegister apply_avx_movshdup128(XMMRegister source) {
    XMMRegister result = {};
    set_xmm_lane_bits(&result, 0, get_xmm_lane_bits(source, 1));
    set_xmm_lane_bits(&result, 1, get_xmm_lane_bits(source, 1));
    set_xmm_lane_bits(&result, 2, get_xmm_lane_bits(source, 3));
    set_xmm_lane_bits(&result, 3, get_xmm_lane_bits(source, 3));
    return result;
}

static AVXRegister256 apply_avx_movshdup256(AVXRegister256 source) {
    AVXRegister256 result = {};
    for (int lane = 0; lane < 8; lane += 2) {
        uint32_t value = get_ymm_lane_bits(source, lane + 1);
        set_ymm_lane_bits(&result, lane, value);
        set_ymm_lane_bits(&result, lane + 1, value);
    }
    return result;
}

static XMMRegister apply_avx_arith_ss128(CPU_CONTEXT* ctx, uint8_t opcode, XMMRegister src1, uint32_t rhs_bits) {
    XMMRegister result = src1;
    float lhs_scalar = sse_bits_to_float(get_xmm_lane_bits(src1, 0));
    float rhs_scalar = sse_bits_to_float(rhs_bits);
    set_xmm_lane_bits(&result, 0, sse_float_to_bits(apply_sse_arith_scalar(ctx, opcode, lhs_scalar, rhs_scalar)));
    return result;
}

static XMMRegister apply_avx_minmax_ss128(uint8_t opcode, XMMRegister src1, uint32_t rhs_bits) {
    XMMRegister rhs_scalar = {};
    set_xmm_lane_bits(&rhs_scalar, 0, rhs_bits);
    __m128 lhs_vec = sse_math_xmm_to_m128(src1);
    __m128 rhs_vec = sse_math_xmm_to_m128(rhs_scalar);
    return sse_math_m128_to_xmm(opcode == 0x5D ? _mm_min_ss(lhs_vec, rhs_vec)
                                                  : _mm_max_ss(lhs_vec, rhs_vec));
}

static XMMRegister apply_avx_sqrt_ss128(XMMRegister src1, uint32_t rhs_bits) {
    XMMRegister result = src1;
    XMMRegister rhs_scalar = {};
    set_xmm_lane_bits(&rhs_scalar, 0, rhs_bits);
    __m128 rhs_vec = sse_math_xmm_to_m128(rhs_scalar);
    set_xmm_lane_bits(&result, 0, sse_math_float_to_bits(_mm_cvtss_f32(_mm_sqrt_ss(rhs_vec))));
    return result;
}

static XMMRegister apply_avx_minmax_pd128(uint8_t opcode, XMMRegister lhs, XMMRegister rhs) {
    __m128d lhs_vec = sse2_math_pd_xmm_to_m128d(lhs);
    __m128d rhs_vec = sse2_math_pd_xmm_to_m128d(rhs);
    return sse2_math_pd_m128d_to_xmm(opcode == 0x5D ? _mm_min_pd(lhs_vec, rhs_vec)
                                                   : _mm_max_pd(lhs_vec, rhs_vec));
}

static AVXRegister256 apply_avx_minmax_pd256(uint8_t opcode, AVXRegister256 lhs, AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_minmax_pd128(opcode, lhs.low, rhs.low);
    result.high = apply_avx_minmax_pd128(opcode, lhs.high, rhs.high);
    return result;
}

static XMMRegister apply_avx_minmax_sd128(uint8_t opcode, XMMRegister src1, uint64_t rhs_bits) {
    XMMRegister rhs_scalar = {};
    rhs_scalar.low = rhs_bits;
    __m128d lhs_vec = sse2_math_pd_xmm_to_m128d(src1);
    __m128d rhs_vec = sse2_math_pd_xmm_to_m128d(rhs_scalar);
    return sse2_math_pd_m128d_to_xmm(opcode == 0x5D ? _mm_min_sd(lhs_vec, rhs_vec)
                                                   : _mm_max_sd(lhs_vec, rhs_vec));
}

static XMMRegister apply_avx_sqrt_pd128(XMMRegister rhs) {
    return sse2_math_pd_m128d_to_xmm(_mm_sqrt_pd(sse2_math_pd_xmm_to_m128d(rhs)));
}

static AVXRegister256 apply_avx_sqrt_pd256(AVXRegister256 rhs) {
    AVXRegister256 result = {};
    result.low = apply_avx_sqrt_pd128(rhs.low);
    result.high = apply_avx_sqrt_pd128(rhs.high);
    return result;
}

static XMMRegister apply_avx_sqrt_sd128(XMMRegister src1, uint64_t rhs_bits) {
    XMMRegister rhs_scalar = {};
    rhs_scalar.low = rhs_bits;
    return sse2_math_pd_m128d_to_xmm(_mm_sqrt_sd(sse2_math_pd_xmm_to_m128d(src1), sse2_math_pd_xmm_to_m128d(rhs_scalar)));
}

