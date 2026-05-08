// instrusments/sse2_scalar_convert.hpp - scalar SSE2 conversion implementation

int decode_sse2_scalar_convert_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_scalar_convert_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_sse2_scalar_convert_gpr_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_scalar_convert_gpr_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_sse2_scalar_convert_mm_reg_index(uint8_t modrm) {
    return (modrm >> 3) & 0x07;
}

int decode_sse2_scalar_convert_mm_rm_index(uint8_t modrm) {
    return modrm & 0x07;
}

void decode_modrm_sse2_scalar_convert(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse2_scalar_convert_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x2A && inst.opcode != 0x2C && inst.opcode != 0x2D && inst.opcode != 0x5A) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (inst.opcode == 0x5A) {
        if (*mandatory_prefix != 0xF2 && *mandatory_prefix != 0xF3) {
            raise_ud_ctx(ctx);
        }
    }
    else if (*mandatory_prefix != 0xF2 && *mandatory_prefix != 0x66) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse2_scalar_convert(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

int64_t sse2_scalar_convert_read_signed_integer_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst, int source_bits) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        int source = decode_sse2_scalar_convert_gpr_rm_index(ctx, inst->modrm);
        if (source_bits == 64) {
            return (int64_t)get_reg64(ctx, source);
        }
        return (int64_t)(int32_t)get_reg32(ctx, source);
    }

    if (source_bits == 64) {
        return (int64_t)read_memory_qword(ctx, inst->mem_address);
    }
    return (int64_t)(int32_t)read_memory_dword(ctx, inst->mem_address);
}

uint64_t sse2_scalar_convert_read_scalar_double_bits(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_scalar_convert_xmm_rm_index(ctx, inst->modrm)).low;
    }
    return read_memory_qword(ctx, inst->mem_address);
}

uint32_t sse2_scalar_convert_read_scalar_float_bits(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return (uint32_t)(get_xmm128(ctx, decode_sse2_scalar_convert_xmm_rm_index(ctx, inst->modrm)).low & 0xFFFFFFFFU);
    }
    return read_memory_dword(ctx, inst->mem_address);
}

uint64_t sse2_scalar_convert_read_mmx_packed_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_mm64(ctx, decode_sse2_scalar_convert_mm_rm_index(inst->modrm));
    }
    return read_memory_qword(ctx, inst->mem_address);
}

XMMRegister sse2_scalar_convert_read_packed_double_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_scalar_convert_xmm_rm_index(ctx, inst->modrm));
    }
    return read_xmm_memory(ctx, inst->mem_address);
}

void sse2_scalar_convert_write_integer_destination(CPU_CONTEXT* ctx, int dest, int dest_bits, bool success, int64_t value) {
    if (dest_bits == 64) {
        set_reg64(ctx, dest, success ? (uint64_t)value : 0x8000000000000000ULL);
        return;
    }

    set_reg32(ctx, dest, success ? (uint32_t)((int32_t)value) : 0x80000000U);
}

inline void execute_sse2_scalar_convert_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const uint8_t mandatory_prefix = inst.mandatory_prefix;

    if (inst.opcode == 0x5A) {
        int dest = decode_sse2_scalar_convert_xmm_reg_index(ctx, inst.modrm);
        XMMRegister result = get_xmm128(ctx, dest);

        if (mandatory_prefix == 0xF3) {
            uint32_t source_bits = sse2_scalar_convert_read_scalar_float_bits(ctx, &inst);
            result.low = sse2_convert_double_to_bits((double)sse_convert_bits_to_float(source_bits));
        }
        else {
            uint64_t source_bits = sse2_scalar_convert_read_scalar_double_bits(ctx, &inst);
            set_xmm_lane_bits(&result, 0, sse_convert_float_to_bits((float)sse2_convert_bits_to_double(source_bits)));
        }

        set_xmm128(ctx, dest, result);
        return;
    }

    if (mandatory_prefix == 0x66) {
        if (inst.opcode == 0x2A) {
            int dest = decode_sse2_scalar_convert_xmm_reg_index(ctx, inst.modrm);
            uint64_t packed = sse2_scalar_convert_read_mmx_packed_source(ctx, &inst);
            int32_t lane0 = (int32_t)(packed & 0xFFFFFFFFU);
            int32_t lane1 = (int32_t)(packed >> 32);
            XMMRegister result = {};
            result.low = sse2_convert_double_to_bits((double)lane0);
            result.high = sse2_convert_double_to_bits((double)lane1);
            set_xmm128(ctx, dest, result);
            return;
        }

        int dest = decode_sse2_scalar_convert_mm_reg_index(inst.modrm);
        XMMRegister source = sse2_scalar_convert_read_packed_double_source(ctx, &inst);
        double lane0_source = sse2_convert_bits_to_double(source.low);
        double lane1_source = sse2_convert_bits_to_double(source.high);
        int64_t lane0_value = 0;
        int64_t lane1_value = 0;
        bool truncate = inst.opcode == 0x2C;
        bool lane0_ok = sse2_convert_round_fp_to_integer(lane0_source, 32, truncate, ctx->mxcsr, &lane0_value);
        bool lane1_ok = sse2_convert_round_fp_to_integer(lane1_source, 32, truncate, ctx->mxcsr, &lane1_value);
        uint32_t lane0_bits = lane0_ok ? (uint32_t)((int32_t)lane0_value) : 0x80000000U;
        uint32_t lane1_bits = lane1_ok ? (uint32_t)((int32_t)lane1_value) : 0x80000000U;
        set_mm64(ctx, dest, (uint64_t)lane0_bits | ((uint64_t)lane1_bits << 32));
        return;
    }

    if (inst.opcode == 0x2A) {
        int dest = decode_sse2_scalar_convert_xmm_reg_index(ctx, inst.modrm);
        int source_bits = ctx->rex_w ? 64 : 32;
        int64_t source_value = sse2_scalar_convert_read_signed_integer_source(ctx, &inst, source_bits);
        XMMRegister result = get_xmm128(ctx, dest);
        result.low = sse2_convert_double_to_bits((double)source_value);
        set_xmm128(ctx, dest, result);
        return;
    }

    int dest_bits = ctx->rex_w ? 64 : 32;
    int dest = decode_sse2_scalar_convert_gpr_reg_index(ctx, inst.modrm);
    double source = sse2_convert_bits_to_double(sse2_scalar_convert_read_scalar_double_bits(ctx, &inst));
    int64_t integer_value = 0;
    bool success = sse2_convert_round_fp_to_integer(source, dest_bits, inst.opcode == 0x2C, ctx->mxcsr, &integer_value);
    sse2_scalar_convert_write_integer_destination(ctx, dest, dest_bits, success, integer_value);
}

inline DecodedInstruction decode_sse2_scalar_convert_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_scalar_convert_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_scalar_convert(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_scalar_convert_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    execute_sse2_scalar_convert_with_decoded(ctx, &inst);
}

inline void execute_sse2_scalar_convert_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_scalar_convert_with_decoded(ctx, &dec->cached);
}
