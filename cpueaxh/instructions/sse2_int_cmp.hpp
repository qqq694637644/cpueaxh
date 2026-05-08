// instrusments/sse2_int_cmp.hpp - PCMPEQ*/PCMPGT* implementation

int decode_sse2_int_cmp_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_int_cmp_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_int_cmp(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse2_int_cmp_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x64 && inst.opcode != 0x65 && inst.opcode != 0x66 &&
        inst.opcode != 0x74 && inst.opcode != 0x75 && inst.opcode != 0x76) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (*mandatory_prefix != 0x66) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse2_int_cmp(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void sse2_int_cmp_xmm_to_bytes(XMMRegister value, uint8_t bytes[16]) {
    for (int index = 0; index < 8; index++) {
        bytes[index] = (uint8_t)((value.low >> (index * 8)) & 0xFFU);
        bytes[8 + index] = (uint8_t)((value.high >> (index * 8)) & 0xFFU);
    }
}

XMMRegister sse2_int_cmp_bytes_to_xmm(const uint8_t bytes[16]) {
    XMMRegister value = {};
    for (int index = 0; index < 8; index++) {
        value.low |= ((uint64_t)bytes[index]) << (index * 8);
        value.high |= ((uint64_t)bytes[8 + index]) << (index * 8);
    }
    return value;
}

uint16_t sse2_int_cmp_read_u16(const uint8_t bytes[16], int index) {
    int offset = index * 2;
    return (uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1] << 8);
}

uint32_t sse2_int_cmp_read_u32(const uint8_t bytes[16], int index) {
    int offset = index * 4;
    return (uint32_t)bytes[offset]
         | ((uint32_t)bytes[offset + 1] << 8)
         | ((uint32_t)bytes[offset + 2] << 16)
         | ((uint32_t)bytes[offset + 3] << 24);
}

int16_t sse2_int_cmp_read_i16(const uint8_t bytes[16], int index) {
    return (int16_t)sse2_int_cmp_read_u16(bytes, index);
}

int32_t sse2_int_cmp_read_i32(const uint8_t bytes[16], int index) {
    return (int32_t)sse2_int_cmp_read_u32(bytes, index);
}

void sse2_int_cmp_write_u16(uint8_t bytes[16], int index, uint16_t value) {
    int offset = index * 2;
    bytes[offset] = (uint8_t)(value & 0xFFU);
    bytes[offset + 1] = (uint8_t)((value >> 8) & 0xFFU);
}

void sse2_int_cmp_write_u32(uint8_t bytes[16], int index, uint32_t value) {
    int offset = index * 4;
    bytes[offset] = (uint8_t)(value & 0xFFU);
    bytes[offset + 1] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[offset + 2] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[offset + 3] = (uint8_t)((value >> 24) & 0xFFU);
}

XMMRegister read_sse2_int_cmp_source_operand(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_int_cmp_xmm_rm_index(ctx, inst->modrm));
    }

    return read_xmm_memory(ctx, inst->mem_address);
}

inline void execute_sse2_int_cmp_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    int dest = decode_sse2_int_cmp_xmm_reg_index(ctx, inst.modrm);
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};

    sse2_int_cmp_xmm_to_bytes(get_xmm128(ctx, dest), lhs_bytes);
    sse2_int_cmp_xmm_to_bytes(read_sse2_int_cmp_source_operand(ctx, &inst), rhs_bytes);

    switch (inst.opcode) {
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

    set_xmm128(ctx, dest, sse2_int_cmp_bytes_to_xmm(result_bytes));
}

inline DecodedInstruction decode_sse2_int_cmp_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_int_cmp_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_int_cmp(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_int_cmp_instruction(ctx, code, code_size, &mandatory_prefix);
    execute_sse2_int_cmp_with_decoded(ctx, &inst);
}

inline void execute_sse2_int_cmp_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_int_cmp_with_decoded(ctx, &dec->cached);
}
