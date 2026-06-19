#pragma once

// instrusments/sse2_arith_pd.hpp - ADDPD/ADDSD/SUBPD/SUBSD/MULPD/MULSD/DIVPD/DIVSD implementation

int decode_sse2_arith_pd_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_arith_pd_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_arith_pd(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse2_arith_pd_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x58 && inst.opcode != 0x59 && inst.opcode != 0x5C && inst.opcode != 0x5E) {
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

    decode_modrm_sse2_arith_pd(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

uint64_t get_sse2_arith_pd_lane_bits(XMMRegister value, int lane) {
    return lane == 0 ? value.low : value.high;
}

void set_sse2_arith_pd_lane_bits(XMMRegister* value, int lane, uint64_t bits) {
    if (lane == 0) {
        value->low = bits;
    }
    else {
        value->high = bits;
    }
}

double sse2_arith_pd_bits_to_double(uint64_t bits) {
    double value = 0.0;
    CPUEAXH_MEMCPY(&value, &bits, sizeof(value));
    return value;
}

uint64_t sse2_arith_pd_double_to_bits(double value) {
    uint64_t bits = 0;
    CPUEAXH_MEMCPY(&bits, &value, sizeof(bits));
    return bits;
}

double apply_sse2_arith_pd_scalar(CPU_CONTEXT* ctx, uint8_t opcode, double lhs, double rhs) {
    switch (opcode) {
    case 0x58: return lhs + rhs;
    case 0x59: return lhs * rhs;
    case 0x5C: return lhs - rhs;
    case 0x5E: return lhs / rhs;
    default: raise_ud_ctx(ctx); return 0.0;
    }
}

XMMRegister read_sse2_arith_pd_packed_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_arith_pd_xmm_rm_index(ctx, inst->modrm));
    }

    return read_xmm_memory(ctx, inst->mem_address);
}

uint64_t read_sse2_arith_pd_scalar_source_bits(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_arith_pd_xmm_rm_index(ctx, inst->modrm)).low;
    }

    return read_memory_qword(ctx, inst->mem_address);
}

inline void execute_sse2_arith_pd_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const uint8_t mandatory_prefix = inst.mandatory_prefix;
    int dest = decode_sse2_arith_pd_xmm_reg_index(ctx, inst.modrm);
    XMMRegister lhs = get_xmm128(ctx, dest);

    if (mandatory_prefix == 0xF2) {
        XMMRegister result = lhs;
        double lhs0 = sse2_arith_pd_bits_to_double(lhs.low);
        double rhs0 = sse2_arith_pd_bits_to_double(read_sse2_arith_pd_scalar_source_bits(ctx, &inst));
        result.low = sse2_arith_pd_double_to_bits(apply_sse2_arith_pd_scalar(ctx, inst.opcode, lhs0, rhs0));
        set_xmm128(ctx, dest, result);
        return;
    }

    XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
    XMMRegister result = {};
    for (int lane = 0; lane < 2; lane++) {
        double lhs_lane = sse2_arith_pd_bits_to_double(get_sse2_arith_pd_lane_bits(lhs, lane));
        double rhs_lane = sse2_arith_pd_bits_to_double(get_sse2_arith_pd_lane_bits(rhs, lane));
        set_sse2_arith_pd_lane_bits(&result, lane, sse2_arith_pd_double_to_bits(apply_sse2_arith_pd_scalar(ctx, inst.opcode, lhs_lane, rhs_lane)));
    }

    set_xmm128(ctx, dest, result);
}

inline DecodedInstruction decode_sse2_arith_pd_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_arith_pd_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_arith_pd(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_arith_pd_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    execute_sse2_arith_pd_with_decoded(ctx, &inst);
}

inline void execute_sse2_arith_pd_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_arith_pd_with_decoded(ctx, &dec->cached);
}
