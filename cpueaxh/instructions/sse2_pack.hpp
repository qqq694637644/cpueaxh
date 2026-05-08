// instrusments/sse2_pack.hpp - PUNPCK*/PACKSS*/PACKUSWB implementation

int decode_sse2_pack_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_pack_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_pack(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse2_pack_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x60 && inst.opcode != 0x61 && inst.opcode != 0x62 && inst.opcode != 0x63 &&
        inst.opcode != 0x67 && inst.opcode != 0x68 && inst.opcode != 0x69 && inst.opcode != 0x6A &&
        inst.opcode != 0x6B && inst.opcode != 0x6C && inst.opcode != 0x6D) {
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

    decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void sse2_pack_xmm_to_bytes(XMMRegister value, uint8_t bytes[16]) {
    for (int i = 0; i < 8; i++) {
        bytes[i] = (uint8_t)((value.low >> (i * 8)) & 0xFFU);
        bytes[8 + i] = (uint8_t)((value.high >> (i * 8)) & 0xFFU);
    }
}

XMMRegister sse2_pack_bytes_to_xmm(const uint8_t bytes[16]) {
    XMMRegister value = {};
    for (int i = 0; i < 8; i++) {
        value.low |= ((uint64_t)bytes[i]) << (i * 8);
        value.high |= ((uint64_t)bytes[8 + i]) << (i * 8);
    }
    return value;
}

int16_t sse2_pack_read_i16(const uint8_t bytes[16], int index) {
    int offset = index * 2;
    return (int16_t)((uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1] << 8));
}

int32_t sse2_pack_read_i32(const uint8_t bytes[16], int index) {
    int offset = index * 4;
    return (int32_t)((uint32_t)bytes[offset]
                   | ((uint32_t)bytes[offset + 1] << 8)
                   | ((uint32_t)bytes[offset + 2] << 16)
                   | ((uint32_t)bytes[offset + 3] << 24));
}

void sse2_pack_write_i16(uint8_t bytes[16], int index, int16_t value) {
    int offset = index * 2;
    uint16_t raw = (uint16_t)value;
    bytes[offset] = (uint8_t)(raw & 0xFFU);
    bytes[offset + 1] = (uint8_t)((raw >> 8) & 0xFFU);
}

int8_t sse2_pack_saturate_i16_to_i8(int16_t value) {
    if (value > 127) {
        return 127;
    }
    if (value < -128) {
        return -128;
    }
    return (int8_t)value;
}

uint8_t sse2_pack_saturate_i16_to_u8(int16_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

int16_t sse2_pack_saturate_i32_to_i16(int32_t value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return (int16_t)value;
}

XMMRegister read_sse2_pack_source_operand(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_pack_xmm_rm_index(ctx, inst->modrm));
    }

    return read_xmm_memory(ctx, inst->mem_address);
}

void sse2_pack_interleave(const uint8_t lhs[16], const uint8_t rhs[16], int unit_size, bool high, uint8_t out[16]) {
    CPUEAXH_MEMSET(out, 0, 16);
    int selected_units = 8 / unit_size;
    int start = high ? selected_units : 0;
    for (int i = 0; i < selected_units; i++) {
        CPUEAXH_MEMCPY(out + (2 * i) * unit_size, lhs + (start + i) * unit_size, unit_size);
        CPUEAXH_MEMCPY(out + (2 * i + 1) * unit_size, rhs + (start + i) * unit_size, unit_size);
    }
}

inline void execute_sse2_pack_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    int dest = decode_sse2_pack_xmm_reg_index(ctx, inst.modrm);
    uint8_t lhs_bytes[16] = {};
    uint8_t rhs_bytes[16] = {};
    uint8_t result_bytes[16] = {};

    sse2_pack_xmm_to_bytes(get_xmm128(ctx, dest), lhs_bytes);
    sse2_pack_xmm_to_bytes(read_sse2_pack_source_operand(ctx, &inst), rhs_bytes);

    switch (inst.opcode) {
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
        for (int i = 0; i < 8; i++) {
            result_bytes[i] = (uint8_t)sse2_pack_saturate_i16_to_i8(sse2_pack_read_i16(lhs_bytes, i));
            result_bytes[8 + i] = (uint8_t)sse2_pack_saturate_i16_to_i8(sse2_pack_read_i16(rhs_bytes, i));
        }
        break;
    case 0x67:
        for (int i = 0; i < 8; i++) {
            result_bytes[i] = sse2_pack_saturate_i16_to_u8(sse2_pack_read_i16(lhs_bytes, i));
            result_bytes[8 + i] = sse2_pack_saturate_i16_to_u8(sse2_pack_read_i16(rhs_bytes, i));
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
        for (int i = 0; i < 4; i++) {
            sse2_pack_write_i16(result_bytes, i, sse2_pack_saturate_i32_to_i16(sse2_pack_read_i32(lhs_bytes, i)));
            sse2_pack_write_i16(result_bytes, 4 + i, sse2_pack_saturate_i32_to_i16(sse2_pack_read_i32(rhs_bytes, i)));
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

    set_xmm128(ctx, dest, sse2_pack_bytes_to_xmm(result_bytes));
}

inline DecodedInstruction decode_sse2_pack_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_pack_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_pack(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_pack_instruction(ctx, code, code_size, &mandatory_prefix);
    execute_sse2_pack_with_decoded(ctx, &inst);
}

inline void execute_sse2_pack_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_pack_with_decoded(ctx, &dec->cached);
}
