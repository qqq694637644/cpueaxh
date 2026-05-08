// instrusments/sse_shuffle.hpp - SHUFPS/UNPCKLPS/UNPCKHPS implementation

int decode_sse_shuffle_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse_shuffle_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse_shuffle(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse_shuffle_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, bool* has_imm8) {
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

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            has_unsupported_simd_prefix = true;
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
    if (inst.opcode != 0x14 && inst.opcode != 0x15 && inst.opcode != 0xC6) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse_shuffle(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    *has_imm8 = inst.opcode == 0xC6;
    if (*has_imm8) {
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.imm_size = 1;
        inst.immediate = code[offset++];
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

uint32_t get_sse_shuffle_lane_bits(XMMRegister value, int lane) {
    switch (lane) {
    case 0: return (uint32_t)(value.low & 0xFFFFFFFFU);
    case 1: return (uint32_t)(value.low >> 32);
    case 2: return (uint32_t)(value.high & 0xFFFFFFFFU);
    default: return (uint32_t)(value.high >> 32);
    }
}

void set_sse_shuffle_lane_bits(XMMRegister* value, int lane, uint32_t bits) {
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

XMMRegister read_sse_shuffle_source_operand(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse_shuffle_xmm_rm_index(ctx, inst->modrm));
    }

    return read_xmm_memory(ctx, inst->mem_address);
}

inline void execute_sse_shuffle_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    int dest = decode_sse_shuffle_xmm_reg_index(ctx, inst.modrm);
    XMMRegister lhs = get_xmm128(ctx, dest);
    XMMRegister rhs = read_sse_shuffle_source_operand(ctx, &inst);
    XMMRegister result = {};

    if (inst.opcode == 0x14) {
        set_sse_shuffle_lane_bits(&result, 0, get_sse_shuffle_lane_bits(lhs, 0));
        set_sse_shuffle_lane_bits(&result, 1, get_sse_shuffle_lane_bits(rhs, 0));
        set_sse_shuffle_lane_bits(&result, 2, get_sse_shuffle_lane_bits(lhs, 1));
        set_sse_shuffle_lane_bits(&result, 3, get_sse_shuffle_lane_bits(rhs, 1));
        set_xmm128(ctx, dest, result);
        return;
    }

    if (inst.opcode == 0x15) {
        set_sse_shuffle_lane_bits(&result, 0, get_sse_shuffle_lane_bits(lhs, 2));
        set_sse_shuffle_lane_bits(&result, 1, get_sse_shuffle_lane_bits(rhs, 2));
        set_sse_shuffle_lane_bits(&result, 2, get_sse_shuffle_lane_bits(lhs, 3));
        set_sse_shuffle_lane_bits(&result, 3, get_sse_shuffle_lane_bits(rhs, 3));
        set_xmm128(ctx, dest, result);
        return;
    }

    if (inst.opcode == 0xC6) {
        uint8_t imm8 = (uint8_t)inst.immediate;
        set_sse_shuffle_lane_bits(&result, 0, get_sse_shuffle_lane_bits(lhs, imm8 & 0x03));
        set_sse_shuffle_lane_bits(&result, 1, get_sse_shuffle_lane_bits(lhs, (imm8 >> 2) & 0x03));
        set_sse_shuffle_lane_bits(&result, 2, get_sse_shuffle_lane_bits(rhs, (imm8 >> 4) & 0x03));
        set_sse_shuffle_lane_bits(&result, 3, get_sse_shuffle_lane_bits(rhs, (imm8 >> 6) & 0x03));
        set_xmm128(ctx, dest, result);
        return;
    }

    raise_ud_ctx(ctx);
}

inline DecodedInstruction decode_sse_shuffle_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    bool has_imm8 = false;
    DecodedInstruction inst = decode_sse_shuffle_instruction(ctx, code, code_size, &has_imm8);
    return inst;
}

void execute_sse_shuffle(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    bool has_imm8 = false;
    DecodedInstruction inst = decode_sse_shuffle_instruction(ctx, code, code_size, &has_imm8);
    execute_sse_shuffle_with_decoded(ctx, &inst);
}

inline void execute_sse_shuffle_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse_shuffle_with_decoded(ctx, &dec->cached);
}
