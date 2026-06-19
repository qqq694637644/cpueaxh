#pragma once

// instrusments/sse2_math_pd.hpp - SQRTPD/SQRTSD/MAXPD/MAXSD/MINPD/MINSD implementation

int decode_sse2_math_pd_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_math_pd_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_math_pd(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
    if (*offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return;
    }

    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    uint8_t mod = (inst->modrm >> 6) & 0x03;
    uint8_t rm = inst->modrm & 0x07;

    if (mod != 3 && rm == 4 && inst->address_size != 16) {
        if (*offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }
        inst->has_sib = true;
        inst->sib = code[(*offset)++];
    }

    if (mod == 0 && rm == 5) {
        inst->disp_size = (inst->address_size == 16) ? 2 : 4;
    }
    else if (mod == 0 && inst->has_sib && (inst->sib & 0x07) == 5) {
        inst->disp_size = 4;
    }
    else if (mod == 1) {
        inst->disp_size = 1;
    }
    else if (mod == 2) {
        inst->disp_size = (inst->address_size == 16) ? 2 : 4;
    }

    if (inst->disp_size > 0) {
        if (*offset + inst->disp_size > code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }

        inst->displacement = 0;
        for (int i = 0; i < inst->disp_size; i++) {
            inst->displacement |= ((int32_t)code[(*offset)++]) << (i * 8);
        }

        if (inst->disp_size == 1) {
            inst->displacement = (int8_t)inst->displacement;
        }
        else if (inst->disp_size == 2) {
            inst->displacement = (int16_t)inst->displacement;
        }
    }

    if (mod != 3) {
        inst->mem_address = get_effective_address(ctx, inst->modrm, &inst->sib, &inst->displacement, inst->address_size);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }
}

DecodedInstruction decode_sse2_math_pd_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
    DecodedInstruction inst = {};
    size_t offset = 0;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

    bool has_lock_prefix = false;
    bool has_unsupported_simd_prefix = false;
    *mandatory_prefix = 0;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            if (*mandatory_prefix == 0 || *mandatory_prefix == prefix) {
                *mandatory_prefix = prefix;
            }
            else {
                has_unsupported_simd_prefix = true;
            }
            offset++;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            offset++;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            ctx->rex_present = true;
            ctx->rex_w = (prefix >> 3) & 1;
            ctx->rex_r = (prefix >> 2) & 1;
            ctx->rex_x = (prefix >> 1) & 1;
            ctx->rex_b = prefix & 1;
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

    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0x51 && inst.opcode != 0x5D && inst.opcode != 0x5F) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (*mandatory_prefix != 0x66 && *mandatory_prefix != 0xF2) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse2_math_pd(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

uint64_t get_sse2_math_pd_lane_bits(XMMRegister value, int lane) {
    return lane == 0 ? value.low : value.high;
}

void set_sse2_math_pd_lane_bits(XMMRegister* value, int lane, uint64_t bits) {
    if (lane == 0) {
        value->low = bits;
    }
    else {
        value->high = bits;
    }
}

double sse2_math_pd_bits_to_double(uint64_t bits) {
    double value = 0.0;
    CPUEAXH_MEMCPY(&value, &bits, sizeof(value));
    return value;
}

uint64_t sse2_math_pd_double_to_bits(double value) {
    uint64_t bits = 0;
    CPUEAXH_MEMCPY(&bits, &value, sizeof(bits));
    return bits;
}

__m128d sse2_math_pd_xmm_to_m128d(XMMRegister value) {
    alignas(16) double lanes[2] = {
        sse2_math_pd_bits_to_double(value.low),
        sse2_math_pd_bits_to_double(value.high)
    };
    return _mm_loadu_pd(lanes);
}

XMMRegister sse2_math_pd_m128d_to_xmm(__m128d value) {
    alignas(16) double lanes[2] = {};
    XMMRegister result = {};
    _mm_storeu_pd(lanes, value);
    result.low = sse2_math_pd_double_to_bits(lanes[0]);
    result.high = sse2_math_pd_double_to_bits(lanes[1]);
    return result;
}

XMMRegister read_sse2_math_pd_packed_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_math_pd_xmm_rm_index(ctx, inst->modrm));
    }

    return read_xmm_memory(ctx, inst->mem_address);
}

uint64_t read_sse2_math_pd_scalar_source_bits(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_math_pd_xmm_rm_index(ctx, inst->modrm)).low;
    }

    return read_memory_qword(ctx, inst->mem_address);
}

inline void execute_sse2_math_pd_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const uint8_t mandatory_prefix = inst.mandatory_prefix;
    int dest = decode_sse2_math_pd_xmm_reg_index(ctx, inst.modrm);
    XMMRegister lhs = get_xmm128(ctx, dest);

    if (inst.opcode == 0x5D || inst.opcode == 0x5F) {
        bool is_min = inst.opcode == 0x5D;
        if (mandatory_prefix == 0xF2) {
            XMMRegister rhs_scalar = {};
            rhs_scalar.low = read_sse2_math_pd_scalar_source_bits(ctx, &inst);
            __m128d lhs_vec = sse2_math_pd_xmm_to_m128d(lhs);
            __m128d rhs_vec = sse2_math_pd_xmm_to_m128d(rhs_scalar);
            __m128d math_result = is_min ? _mm_min_sd(lhs_vec, rhs_vec) : _mm_max_sd(lhs_vec, rhs_vec);
            set_xmm128(ctx, dest, sse2_math_pd_m128d_to_xmm(math_result));
            return;
        }

        XMMRegister rhs = read_sse2_math_pd_packed_source(ctx, &inst);
        __m128d lhs_vec = sse2_math_pd_xmm_to_m128d(lhs);
        __m128d rhs_vec = sse2_math_pd_xmm_to_m128d(rhs);
        set_xmm128(ctx, dest, sse2_math_pd_m128d_to_xmm(is_min ? _mm_min_pd(lhs_vec, rhs_vec) : _mm_max_pd(lhs_vec, rhs_vec)));
        return;
    }

    if (mandatory_prefix == 0xF2) {
        XMMRegister rhs_scalar = {};
        rhs_scalar.low = read_sse2_math_pd_scalar_source_bits(ctx, &inst);
        __m128d result = _mm_sqrt_sd(sse2_math_pd_xmm_to_m128d(lhs), sse2_math_pd_xmm_to_m128d(rhs_scalar));
        set_xmm128(ctx, dest, sse2_math_pd_m128d_to_xmm(result));
        return;
    }

    XMMRegister rhs = read_sse2_math_pd_packed_source(ctx, &inst);
    set_xmm128(ctx, dest, sse2_math_pd_m128d_to_xmm(_mm_sqrt_pd(sse2_math_pd_xmm_to_m128d(rhs))));
}

inline DecodedInstruction decode_sse2_math_pd_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_math_pd_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_math_pd(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_math_pd_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    execute_sse2_math_pd_with_decoded(ctx, &inst);
}

inline void execute_sse2_math_pd_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_math_pd_with_decoded(ctx, &dec->cached);
}
