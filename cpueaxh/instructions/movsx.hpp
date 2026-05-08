// instrusments/movsx.hpp - MOVSX instruction implementation

int decode_movsx_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_movsx_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

uint64_t read_movsx_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_movsx_rm_index(ctx, modrm);

    if (mod == 3) {
        switch (operand_size) {
        case 8:  return get_reg8(ctx, rm);
        case 16: return get_reg16(ctx, rm);
        default: raise_ud_ctx(ctx); return 0;
        }
    }

    switch (operand_size) {
    case 8:  return read_memory_byte(ctx, mem_addr);
    case 16: return read_memory_word(ctx, mem_addr);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_movsx_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size, int64_t value) {
    int reg = decode_movsx_reg_index(ctx, modrm);

    switch (operand_size) {
    case 16:
        set_reg16(ctx, reg, (uint16_t)value);
        break;
    case 32:
        set_reg32(ctx, reg, (uint32_t)value);
        break;
    case 64:
        set_reg64(ctx, reg, (uint64_t)value);
        break;
    default:
        raise_ud_ctx(ctx);
    }
}

void movsx_r16_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    int8_t value = (int8_t)read_movsx_rm_operand(ctx, modrm, mem_addr, 8);
    write_movsx_reg_operand(ctx, modrm, 16, value);
}

void movsx_r32_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    int8_t value = (int8_t)read_movsx_rm_operand(ctx, modrm, mem_addr, 8);
    write_movsx_reg_operand(ctx, modrm, 32, value);
}

void movsx_r64_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    int8_t value = (int8_t)read_movsx_rm_operand(ctx, modrm, mem_addr, 8);
    write_movsx_reg_operand(ctx, modrm, 64, value);
}

void movsx_r32_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    int16_t value = (int16_t)read_movsx_rm_operand(ctx, modrm, mem_addr, 16);
    write_movsx_reg_operand(ctx, modrm, 32, value);
}

void movsx_r64_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    int16_t value = (int16_t)read_movsx_rm_operand(ctx, modrm, mem_addr, 16);
    write_movsx_reg_operand(ctx, modrm, 64, value);
}

void decode_modrm_movsx(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_movsx_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66) {
            ctx->operand_size_override = true;
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
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
            offset++;
        }
        else {
            break;
        }
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0x0F) {
        raise_ud_ctx(ctx);
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0xBE && inst.opcode != 0xBF) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = 32;
    if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    if (inst.opcode == 0xBF && inst.operand_size == 16) {
        raise_ud_ctx(ctx);
    }

    decode_modrm_movsx(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_movsx_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    case 0xBE:
        if (ctx->rex_w) {
            movsx_r64_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            movsx_r16_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            movsx_r32_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;

    case 0xBF:
        if (ctx->rex_w) {
            movsx_r64_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            movsx_r32_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    }
}

void execute_movsx(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_movsx_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_movsx_with_decoded(ctx, &inst);
}

inline void execute_movsx_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_movsx_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_movsx_with_decoded(ctx, &live);
}
