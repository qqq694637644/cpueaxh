#pragma once

// instrusments/sse2_shift.hpp - PSLL*/PSRL*/PSRA* implementation

int decode_sse2_shift_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_shift_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_shift(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_sse2_shift_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x71 && inst.opcode != 0x72 && inst.opcode != 0x73 &&
        inst.opcode != 0xD1 && inst.opcode != 0xD2 && inst.opcode != 0xD3 &&
        inst.opcode != 0xE1 && inst.opcode != 0xE2 &&
        inst.opcode != 0xF1 && inst.opcode != 0xF2 && inst.opcode != 0xF3) {
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

    decode_modrm_sse2_shift(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    uint8_t group = (inst.modrm >> 3) & 0x07;
    if (inst.opcode == 0x71 || inst.opcode == 0x72) {
        if (((inst.modrm >> 6) & 0x03) != 0x03) {
            raise_ud_ctx(ctx);
        }
        if (group != 2 && group != 4 && group != 6) {
            raise_ud_ctx(ctx);
        }
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.imm_size = 1;
        inst.immediate = code[offset++];
    }
    else if (inst.opcode == 0x73) {
        if (((inst.modrm >> 6) & 0x03) != 0x03) {
            raise_ud_ctx(ctx);
        }
        if (group != 2 && group != 3 && group != 6 && group != 7) {
            raise_ud_ctx(ctx);
        }
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

void sse2_shift_xmm_to_bytes(XMMRegister value, uint8_t bytes[16]) {
    for (int index = 0; index < 8; index++) {
        bytes[index] = (uint8_t)((value.low >> (index * 8)) & 0xFFU);
        bytes[8 + index] = (uint8_t)((value.high >> (index * 8)) & 0xFFU);
    }
}

XMMRegister sse2_shift_bytes_to_xmm(const uint8_t bytes[16]) {
    XMMRegister value = {};
    for (int index = 0; index < 8; index++) {
        value.low |= ((uint64_t)bytes[index]) << (index * 8);
        value.high |= ((uint64_t)bytes[8 + index]) << (index * 8);
    }
    return value;
}

uint16_t sse2_shift_read_u16(const uint8_t bytes[16], int index) {
    int offset = index * 2;
    return (uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1] << 8);
}

uint32_t sse2_shift_read_u32(const uint8_t bytes[16], int index) {
    int offset = index * 4;
    return (uint32_t)bytes[offset]
         | ((uint32_t)bytes[offset + 1] << 8)
         | ((uint32_t)bytes[offset + 2] << 16)
         | ((uint32_t)bytes[offset + 3] << 24);
}

uint64_t sse2_shift_read_u64(const uint8_t bytes[16], int index) {
    int offset = index * 8;
    uint64_t value = 0;
    for (int byte_index = 0; byte_index < 8; byte_index++) {
        value |= ((uint64_t)bytes[offset + byte_index]) << (byte_index * 8);
    }
    return value;
}

int16_t sse2_shift_read_i16(const uint8_t bytes[16], int index) {
    return (int16_t)sse2_shift_read_u16(bytes, index);
}

int32_t sse2_shift_read_i32(const uint8_t bytes[16], int index) {
    return (int32_t)sse2_shift_read_u32(bytes, index);
}

void sse2_shift_write_u16(uint8_t bytes[16], int index, uint16_t value) {
    int offset = index * 2;
    bytes[offset] = (uint8_t)(value & 0xFFU);
    bytes[offset + 1] = (uint8_t)((value >> 8) & 0xFFU);
}

void sse2_shift_write_u32(uint8_t bytes[16], int index, uint32_t value) {
    int offset = index * 4;
    bytes[offset] = (uint8_t)(value & 0xFFU);
    bytes[offset + 1] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[offset + 2] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[offset + 3] = (uint8_t)((value >> 24) & 0xFFU);
}

void sse2_shift_write_u64(uint8_t bytes[16], int index, uint64_t value) {
    int offset = index * 8;
    for (int byte_index = 0; byte_index < 8; byte_index++) {
        bytes[offset + byte_index] = (uint8_t)((value >> (byte_index * 8)) & 0xFFU);
    }
}

void sse2_shift_apply_psllw(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count > 15) {
        return;
    }
    for (int index = 0; index < 8; index++) {
        sse2_shift_write_u16(dst, index, (uint16_t)(sse2_shift_read_u16(src, index) << count));
    }
}

void sse2_shift_apply_pslld(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count > 31) {
        return;
    }
    for (int index = 0; index < 4; index++) {
        sse2_shift_write_u32(dst, index, sse2_shift_read_u32(src, index) << count);
    }
}

void sse2_shift_apply_psllq(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count > 63) {
        return;
    }
    for (int index = 0; index < 2; index++) {
        sse2_shift_write_u64(dst, index, sse2_shift_read_u64(src, index) << count);
    }
}

void sse2_shift_apply_psrlw(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count > 15) {
        return;
    }
    for (int index = 0; index < 8; index++) {
        sse2_shift_write_u16(dst, index, (uint16_t)(sse2_shift_read_u16(src, index) >> count));
    }
}

void sse2_shift_apply_psrld(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count > 31) {
        return;
    }
    for (int index = 0; index < 4; index++) {
        sse2_shift_write_u32(dst, index, sse2_shift_read_u32(src, index) >> count);
    }
}

void sse2_shift_apply_psrlq(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count > 63) {
        return;
    }
    for (int index = 0; index < 2; index++) {
        sse2_shift_write_u64(dst, index, sse2_shift_read_u64(src, index) >> count);
    }
}

void sse2_shift_apply_psraw(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    int effective = (count > 15) ? 15 : (int)count;
    for (int index = 0; index < 8; index++) {
        sse2_shift_write_u16(dst, index, (uint16_t)(sse2_shift_read_i16(src, index) >> effective));
    }
}

void sse2_shift_apply_psrad(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    int effective = (count > 31) ? 31 : (int)count;
    for (int index = 0; index < 4; index++) {
        sse2_shift_write_u32(dst, index, (uint32_t)(sse2_shift_read_i32(src, index) >> effective));
    }
}

void sse2_shift_apply_psrldq(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count >= 16) {
        return;
    }
    int shift = (int)count;
    for (int index = 0; index < 16 - shift; index++) {
        dst[index] = src[index + shift];
    }
}

void sse2_shift_apply_pslldq(const uint8_t src[16], uint64_t count, uint8_t dst[16]) {
    CPUEAXH_MEMSET(dst, 0, 16);
    if (count >= 16) {
        return;
    }
    int shift = (int)count;
    for (int index = shift; index < 16; index++) {
        dst[index] = src[index - shift];
    }
}

XMMRegister apply_sse2_psrldq128(XMMRegister value, uint8_t imm8) {
    uint8_t src[16] = {};
    uint8_t dst[16] = {};
    sse2_shift_xmm_to_bytes(value, src);
    sse2_shift_apply_psrldq(src, imm8, dst);
    return sse2_shift_bytes_to_xmm(dst);
}

XMMRegister apply_sse2_pslldq128(XMMRegister value, uint8_t imm8) {
    uint8_t src[16] = {};
    uint8_t dst[16] = {};
    sse2_shift_xmm_to_bytes(value, src);
    sse2_shift_apply_pslldq(src, imm8, dst);
    return sse2_shift_bytes_to_xmm(dst);
}

XMMRegister read_sse2_shift_source_operand(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_sse2_shift_xmm_rm_index(ctx, inst->modrm));
    }

    return read_xmm_memory(ctx, inst->mem_address);
}

int decode_sse2_shift_dest_index(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (inst->opcode == 0x71 || inst->opcode == 0x72 || inst->opcode == 0x73) {
        return decode_sse2_shift_xmm_rm_index(ctx, inst->modrm);
    }
    return decode_sse2_shift_xmm_reg_index(ctx, inst->modrm);
}

inline void execute_sse2_shift_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    int dest = decode_sse2_shift_dest_index(ctx, &inst);
    uint8_t src_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    uint64_t count = 0;

    sse2_shift_xmm_to_bytes(get_xmm128(ctx, dest), src_bytes);

    if (inst.opcode == 0x71 || inst.opcode == 0x72 || inst.opcode == 0x73) {
        count = (uint8_t)inst.immediate;
        uint8_t group = (inst.modrm >> 3) & 0x07;
        if (inst.opcode == 0x71) {
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
        else if (inst.opcode == 0x72) {
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
        XMMRegister count_source = read_sse2_shift_source_operand(ctx, &inst);
        count = count_source.low;
        switch (inst.opcode) {
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

    set_xmm128(ctx, dest, sse2_shift_bytes_to_xmm(result_bytes));
}

inline DecodedInstruction decode_sse2_shift_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_shift_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

void execute_sse2_shift(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_shift_instruction(ctx, code, code_size, &mandatory_prefix);
    execute_sse2_shift_with_decoded(ctx, &inst);
}

inline void execute_sse2_shift_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sse2_shift_with_decoded(ctx, &dec->cached);
}
