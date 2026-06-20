#pragma once

// instrusments/movdq.hpp - MOVD/MOVQ/MOVDQ2Q/MOVQ2DQ instruction implementation

int decode_movdq_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_movdq_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_movdq_gpr_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_movdq_mm_reg_index(uint8_t modrm) {
    return (modrm >> 3) & 0x07;
}

int decode_movdq_mm_rm_index(uint8_t modrm) {
    return modrm & 0x07;
}

void decode_modrm_movdq(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_movdq_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    if (inst.opcode != 0x6E && inst.opcode != 0x7E && inst.opcode != 0xD6) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (inst.opcode == 0x6E && *mandatory_prefix != 0x00 && *mandatory_prefix != 0x66) {
        raise_ud_ctx(ctx);
    }

    if (inst.opcode == 0x7E && *mandatory_prefix != 0x00 && *mandatory_prefix != 0x66 && *mandatory_prefix != 0xF3) {
        raise_ud_ctx(ctx);
    }

    if (inst.opcode == 0xD6 && *mandatory_prefix != 0x66 && *mandatory_prefix != 0xF2 && *mandatory_prefix != 0xF3) {
        raise_ud_ctx(ctx);
    }

    if (inst.opcode != 0xD6 && *mandatory_prefix == 0xF2) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_movdq(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

uint64_t read_movdq_gpr_or_mem(CPU_CONTEXT* ctx, const DecodedInstruction* inst, int operand_size) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        int reg = decode_movdq_gpr_rm_index(ctx, inst->modrm);
        return (operand_size == 64) ? get_reg64(ctx, reg) : get_reg32(ctx, reg);
    }

    return (operand_size == 64) ? read_memory_qword(ctx, inst->mem_address)
                                : read_memory_dword(ctx, inst->mem_address);
}

void write_movdq_gpr_or_mem(CPU_CONTEXT* ctx, const DecodedInstruction* inst, int operand_size, uint64_t value) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        int reg = decode_movdq_gpr_rm_index(ctx, inst->modrm);
        if (operand_size == 64) {
            set_reg64(ctx, reg, value);
        }
        else {
            set_reg32(ctx, reg, (uint32_t)value);
        }
        return;
    }

    if (operand_size == 64) {
        write_memory_qword(ctx, inst->mem_address, value);
    }
    else {
        write_memory_dword(ctx, inst->mem_address, (uint32_t)value);
    }
}

uint64_t read_movdq_xmm_low_or_mem64(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        return get_xmm128(ctx, decode_movdq_xmm_rm_index(ctx, inst->modrm)).low;
    }

    return read_memory_qword(ctx, inst->mem_address);
}

void write_movdq_xmm_low_or_mem64(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint64_t value) {
    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        XMMRegister result = {};
        result.low = value;
        result.high = 0;
        set_xmm128(ctx, decode_movdq_xmm_rm_index(ctx, inst->modrm), result);
        return;
    }

    write_memory_qword(ctx, inst->mem_address, value);
}

inline DecodedInstruction decode_movdq_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_movdq_instruction(ctx, code, code_size, &mandatory_prefix);
    inst.mandatory_prefix = mandatory_prefix;
    return inst;
}

inline void execute_movdq_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    uint8_t mandatory_prefix = inst.mandatory_prefix;

    if (mandatory_prefix == 0x00 && inst.opcode == 0x6E) {
        set_mm64(ctx, decode_movdq_mm_reg_index(inst.modrm), read_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32));
        return;
    }

    if (mandatory_prefix == 0x00 && inst.opcode == 0x7E) {
        write_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32, get_mm64(ctx, decode_movdq_mm_reg_index(inst.modrm)));
        return;
    }

    if (mandatory_prefix == 0x66 && inst.opcode == 0x6E) {
        XMMRegister result = {};
        result.low = read_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32);
        result.high = 0;
        set_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm), result);
        return;
    }

    if (mandatory_prefix == 0x66 && inst.opcode == 0x7E) {
        XMMRegister src = get_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm));
        write_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32, src.low);
        return;
    }

    if (mandatory_prefix == 0xF3 && inst.opcode == 0x7E) {
        XMMRegister result = {};
        result.low = read_movdq_xmm_low_or_mem64(ctx, &inst);
        result.high = 0;
        set_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm), result);
        return;
    }

    if (mandatory_prefix == 0x66 && inst.opcode == 0xD6) {
        XMMRegister src = get_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm));
        write_movdq_xmm_low_or_mem64(ctx, &inst, src.low);
        return;
    }

    if (mandatory_prefix == 0xF2 && inst.opcode == 0xD6) {
        if (((inst.modrm >> 6) & 0x03) != 3) {
            raise_ud_ctx(ctx);
        }

        int dest = decode_movdq_mm_reg_index(inst.modrm);
        int source = decode_movdq_xmm_rm_index(ctx, inst.modrm);
        set_mm64(ctx, dest, get_xmm128(ctx, source).low);
        return;
    }

    if (mandatory_prefix == 0xF3 && inst.opcode == 0xD6) {
        if (((inst.modrm >> 6) & 0x03) != 3) {
            raise_ud_ctx(ctx);
        }

        int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
        int source = decode_movdq_mm_rm_index(inst.modrm);
        XMMRegister result = {};
        result.low = get_mm64(ctx, source);
        result.high = 0;
        set_xmm128(ctx, dest, result);
        return;
    }

    raise_ud_ctx(ctx);
}

inline void execute_movdq_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_movdq_with_decoded(ctx, &dec->cached);
}

void execute_movdq(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_movdq_instruction(ctx, code, code_size, &mandatory_prefix);

    if (mandatory_prefix == 0x00 && inst.opcode == 0x6E) {
        set_mm64(ctx, decode_movdq_mm_reg_index(inst.modrm), read_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32));
        return;
    }

    if (mandatory_prefix == 0x00 && inst.opcode == 0x7E) {
        write_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32, get_mm64(ctx, decode_movdq_mm_reg_index(inst.modrm)));
        return;
    }

    if (mandatory_prefix == 0x66 && inst.opcode == 0x6E) {
        XMMRegister result = {};
        result.low = read_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32);
        result.high = 0;
        set_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm), result);
        return;
    }

    if (mandatory_prefix == 0x66 && inst.opcode == 0x7E) {
        XMMRegister src = get_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm));
        write_movdq_gpr_or_mem(ctx, &inst, ctx->rex_w ? 64 : 32, src.low);
        return;
    }

    if (mandatory_prefix == 0xF3 && inst.opcode == 0x7E) {
        XMMRegister result = {};
        result.low = read_movdq_xmm_low_or_mem64(ctx, &inst);
        result.high = 0;
        set_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm), result);
        return;
    }

    if (mandatory_prefix == 0x66 && inst.opcode == 0xD6) {
        XMMRegister src = get_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst.modrm));
        write_movdq_xmm_low_or_mem64(ctx, &inst, src.low);
        return;
    }

    if (mandatory_prefix == 0xF2 && inst.opcode == 0xD6) {
        if (((inst.modrm >> 6) & 0x03) != 3) {
            raise_ud_ctx(ctx);
        }

        int dest = decode_movdq_mm_reg_index(inst.modrm);
        int source = decode_movdq_xmm_rm_index(ctx, inst.modrm);
        set_mm64(ctx, dest, get_xmm128(ctx, source).low);
        return;
    }

    if (mandatory_prefix == 0xF3 && inst.opcode == 0xD6) {
        if (((inst.modrm >> 6) & 0x03) != 3) {
            raise_ud_ctx(ctx);
        }

        int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
        int source = decode_movdq_mm_rm_index(inst.modrm);
        XMMRegister result = {};
        result.low = get_mm64(ctx, source);
        result.high = 0;
        set_xmm128(ctx, dest, result);
        return;
    }

    raise_ud_ctx(ctx);
}
