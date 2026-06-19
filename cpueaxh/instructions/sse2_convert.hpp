#pragma once

// instrusments/sse2_convert.hpp - packed SSE2 conversion implementation

int decode_sse2_convert_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_convert_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_convert(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse2_convert_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x5A && inst.opcode != 0x5B && inst.opcode != 0xE6) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (inst.opcode == 0x5A) {
        if (*mandatory_prefix != 0 && *mandatory_prefix != 0x66) {
            raise_ud_ctx(ctx);
        }
    }
    else if (inst.opcode == 0x5B) {
        if (*mandatory_prefix == 0xF2) {
            raise_ud_ctx(ctx);
        }
    }
    else {
        if (*mandatory_prefix != 0x66 && *mandatory_prefix != 0xF2 && *mandatory_prefix != 0xF3) {
            raise_ud_ctx(ctx);
        }
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse2_convert(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

uint32_t sse2_convert_get_ps_lane_bits(XMMRegister value, int lane) {
    switch (lane) {
    case 0: return (uint32_t)(value.low & 0xFFFFFFFFU);
    case 1: return (uint32_t)(value.low >> 32);
    case 2: return (uint32_t)(value.high & 0xFFFFFFFFU);
    default: return (uint32_t)(value.high >> 32);
    }
}

void sse2_convert_set_ps_lane_bits(XMMRegister* value, int lane, uint32_t bits) {
    switch (lane) {
    case 0:
        value->low = (value->low & 0xFFFFFFFF00000000ULL) | bits;
        break;
    case 1:
        value->low = (value->low & 0x00000000FFFFFFFFULL) | ((uint64_t)bits << 32);
        break;
    case 2:
        value->high = (value->high & 0xFFFFFFFF00000000ULL) | bits;
        break;
    default:
        value->high = (value->high & 0x00000000FFFFFFFFULL) | ((uint64_t)bits << 32);
        break;
    }
}

uint64_t sse2_convert_get_pd_lane_bits(XMMRegister value, int lane) {
    return lane == 0 ? value.low : value.high;
}

void sse2_convert_set_pd_lane_bits(XMMRegister* value, int lane, uint64_t bits) {
    if (lane == 0) {
        value->low = bits;
    }
    else {
        value->high = bits;
    }
}

float sse2_convert_bits_to_float(uint32_t bits) {
    float value = 0.0f;
    CPUEAXH_MEMCPY(&value, &bits, sizeof(value));
    return value;
}

uint32_t sse2_convert_float_to_bits(float value) {
    uint32_t bits = 0;
    CPUEAXH_MEMCPY(&bits, &value, sizeof(bits));
    return bits;
}

double sse2_convert_bits_to_double(uint64_t bits) {
    double value = 0.0;
    CPUEAXH_MEMCPY(&value, &bits, sizeof(value));
    return value;
}

uint64_t sse2_convert_double_to_bits(double value) {
    uint64_t bits = 0;
    CPUEAXH_MEMCPY(&bits, &value, sizeof(bits));
    return bits;
}

int32_t sse2_convert_bits_to_i32(uint32_t bits) {
    return (int32_t)bits;
}

uint64_t sse2_convert_read_low64_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_convert_xmm_rm_index(ctx, inst->modrm)).low;
    }
    return read_memory_qword(ctx, inst->mem_address);
}

XMMRegister sse2_convert_read_xmm_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_convert_xmm_rm_index(ctx, inst->modrm));
    }
    return read_xmm_memory(ctx, inst->mem_address);
}

bool sse2_convert_round_fp_to_integer(double source, int dest_bits, bool truncate, uint32_t mxcsr, int64_t* result) {
    static const int64_t I64_MIN_VALUE = (-9223372036854775807LL - 1);
    static const int64_t I64_MAX_VALUE =  9223372036854775807LL;
    static const int64_t I32_MIN_VALUE = -2147483648LL;
    static const int64_t I32_MAX_VALUE =  2147483647LL;
    static const double I64_MIN_BOUND = -9223372036854775808.0;
    static const double I64_MAX_EXCLUSIVE = 9223372036854775808.0;

    double value = source;
    if (value != value || value < I64_MIN_BOUND || value >= I64_MAX_EXCLUSIVE) {
        return false;
    }

    int64_t rounded = (int64_t)value;
    double fraction = value - (double)rounded;

    if (!truncate) {
        switch ((mxcsr >> 13) & 0x03) {
        case 0:
            if (fraction > 0.5) {
                if (rounded == I64_MAX_VALUE) {
                    return false;
                }
                rounded++;
            }
            else if (fraction < -0.5) {
                if (rounded == I64_MIN_VALUE) {
                    return false;
                }
                rounded--;
            }
            else if (fraction == 0.5) {
                if ((rounded & 1LL) != 0) {
                    if (rounded == I64_MAX_VALUE) {
                        return false;
                    }
                    rounded++;
                }
            }
            else if (fraction == -0.5) {
                if ((rounded & 1LL) != 0) {
                    if (rounded == I64_MIN_VALUE) {
                        return false;
                    }
                    rounded--;
                }
            }
            break;
        case 1:
            if (fraction < 0.0) {
                if (rounded == I64_MIN_VALUE) {
                    return false;
                }
                rounded--;
            }
            break;
        case 2:
            if (fraction > 0.0) {
                if (rounded == I64_MAX_VALUE) {
                    return false;
                }
                rounded++;
            }
            break;
        case 3:
        default:
            break;
        }
    }

    if (dest_bits == 32 && (rounded < I32_MIN_VALUE || rounded > I32_MAX_VALUE)) {
        return false;
    }

    *result = rounded;
    return true;
}

inline void execute_sse2_convert_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const uint8_t mandatory_prefix = inst.mandatory_prefix;
    int dest = decode_sse2_convert_xmm_reg_index(ctx, inst.modrm);

    if (inst.opcode == 0x5A) {
        if (mandatory_prefix == 0) {
            uint64_t packed = sse2_convert_read_low64_source(ctx, &inst);
            float lane0 = sse2_convert_bits_to_float((uint32_t)(packed & 0xFFFFFFFFU));
            float lane1 = sse2_convert_bits_to_float((uint32_t)(packed >> 32));
            XMMRegister result = {};
            result.low = sse2_convert_double_to_bits((double)lane0);
            result.high = sse2_convert_double_to_bits((double)lane1);
            set_xmm128(ctx, dest, result);
            return;
        }

        XMMRegister source = sse2_convert_read_xmm_source(ctx, &inst);
        XMMRegister result = {};
        sse2_convert_set_ps_lane_bits(&result, 0, sse2_convert_float_to_bits((float)sse2_convert_bits_to_double(source.low)));
        sse2_convert_set_ps_lane_bits(&result, 1, sse2_convert_float_to_bits((float)sse2_convert_bits_to_double(source.high)));
        set_xmm128(ctx, dest, result);
        return;
    }

    if (inst.opcode == 0x5B) {
        XMMRegister source = sse2_convert_read_xmm_source(ctx, &inst);
        XMMRegister result = {};

        if (mandatory_prefix == 0) {
            for (int lane = 0; lane < 4; lane++) {
                int32_t value = sse2_convert_bits_to_i32(sse2_convert_get_ps_lane_bits(source, lane));
                sse2_convert_set_ps_lane_bits(&result, lane, sse2_convert_float_to_bits((float)value));
            }
            set_xmm128(ctx, dest, result);
            return;
        }

        bool truncate = mandatory_prefix == 0xF3;
        for (int lane = 0; lane < 4; lane++) {
            double value = (double)sse2_convert_bits_to_float(sse2_convert_get_ps_lane_bits(source, lane));
            int64_t converted = 0;
            bool success = sse2_convert_round_fp_to_integer(value, 32, truncate, ctx->mxcsr, &converted);
            sse2_convert_set_ps_lane_bits(&result, lane, success ? (uint32_t)((int32_t)converted) : 0x80000000U);
        }
        set_xmm128(ctx, dest, result);
        return;
    }

    if (inst.opcode == 0xE6) {
        if (mandatory_prefix == 0xF3) {
            uint64_t packed = sse2_convert_read_low64_source(ctx, &inst);
            int32_t lane0 = (int32_t)(packed & 0xFFFFFFFFU);
            int32_t lane1 = (int32_t)(packed >> 32);
            XMMRegister result = {};
            result.low = sse2_convert_double_to_bits((double)lane0);
            result.high = sse2_convert_double_to_bits((double)lane1);
            set_xmm128(ctx, dest, result);
            return;
        }

        XMMRegister source = sse2_convert_read_xmm_source(ctx, &inst);
        XMMRegister result = {};
        bool truncate = mandatory_prefix == 0x66;
        for (int lane = 0; lane < 2; lane++) {
            double value = sse2_convert_bits_to_double(sse2_convert_get_pd_lane_bits(source, lane));
            int64_t converted = 0;
            bool success = sse2_convert_round_fp_to_integer(value, 32, truncate, ctx->mxcsr, &converted);
            sse2_convert_set_ps_lane_bits(&result, lane, success ? (uint32_t)((int32_t)converted) : 0x80000000U);
        }
        set_xmm128(ctx, dest, result);
        return;
    }

    raise_ud_ctx(ctx);
}

inline DecodedInstruction decode_sse2_convert_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_convert_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_convert(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_convert_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    execute_sse2_convert_with_decoded(ctx, &inst);
}

inline void execute_sse2_convert_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_convert_with_decoded(ctx, &dec->cached);
}
