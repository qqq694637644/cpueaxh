// instrusments/sar.hpp - SAR instruction implementation

int decode_sar_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

uint64_t read_sar_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_sar_rm_index(ctx, modrm);

    if (mod == 3) {
        switch (operand_size) {
        case 8:  return get_reg8(ctx, rm);
        case 16: return get_reg16(ctx, rm);
        case 32: return get_reg32(ctx, rm);
        case 64: return get_reg64(ctx, rm);
        default: raise_ud_ctx(ctx); return 0;
        }
    }

    switch (operand_size) {
    case 8:  return read_memory_byte(ctx, mem_addr);
    case 16: return read_memory_word(ctx, mem_addr);
    case 32: return read_memory_dword(ctx, mem_addr);
    case 64: return read_memory_qword(ctx, mem_addr);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_sar_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size, uint64_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_sar_rm_index(ctx, modrm);

    if (mod == 3) {
        switch (operand_size) {
        case 8:  set_reg8(ctx, rm, (uint8_t)value); break;
        case 16: set_reg16(ctx, rm, (uint16_t)value); break;
        case 32: set_reg32(ctx, rm, (uint32_t)value); break;
        case 64: set_reg64(ctx, rm, value); break;
        default: raise_ud_ctx(ctx);
        }
        return;
    }

    switch (operand_size) {
    case 8:  write_memory_byte(ctx, mem_addr, (uint8_t)value); break;
    case 16: write_memory_word(ctx, mem_addr, (uint16_t)value); break;
    case 32: write_memory_dword(ctx, mem_addr, (uint32_t)value); break;
    case 64: write_memory_qword(ctx, mem_addr, value); break;
    default: raise_ud_ctx(ctx);
    }
}

uint64_t get_sar_operand_mask(CPU_CONTEXT* ctx, int operand_size) {
    switch (operand_size) {
    case 8:  return 0xFFULL;
    case 16: return 0xFFFFULL;
    case 32: return 0xFFFFFFFFULL;
    case 64: return 0xFFFFFFFFFFFFFFFFULL;
    default: raise_ud_ctx(ctx); return 0;
    }
}

uint64_t get_sar_sign_mask(CPU_CONTEXT* ctx, int operand_size) {
    switch (operand_size) {
    case 8:  return 0x80ULL;
    case 16: return 0x8000ULL;
    case 32: return 0x80000000ULL;
    case 64: return 0x8000000000000000ULL;
    default: raise_ud_ctx(ctx); return 0;
    }
}

uint8_t get_sar_count_mask(int operand_size) {
    return operand_size == 64 ? 0x3F : 0x1F;
}

uint8_t decode_sar_count(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    switch (inst->opcode) {
    case 0xD0:
    case 0xD1:
        return 1;
    case 0xD2:
    case 0xD3:
        return get_reg8(ctx, REG_RCX);
    case 0xC0:
    case 0xC1:
        return (uint8_t)inst->immediate;
    default:
        raise_ud_ctx(ctx);
        return 0;
    }
}

void update_flags_sar(CPU_CONTEXT* ctx, uint64_t result, int operand_size, unsigned int count, bool carry_out) {
    if (count == 0) {
        return;
    }

    set_flag(ctx, RFLAGS_CF, carry_out);
    if (count == 1) {
        set_flag(ctx, RFLAGS_OF, false);
    }
    set_flag(ctx, RFLAGS_SF, (result & get_sar_sign_mask(ctx, operand_size)) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

void sar_rm(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int operand_size, uint8_t raw_count) {
    (void)sib;
    (void)disp;

    uint64_t mask = get_sar_operand_mask(ctx, operand_size);
    uint64_t sign_mask = get_sar_sign_mask(ctx, operand_size);
    unsigned int count = raw_count & get_sar_count_mask(operand_size);
    uint64_t result = read_sar_rm_operand(ctx, modrm, mem_addr, operand_size) & mask;
    if (cpu_has_exception(ctx)) {
        return;
    }

    if (count == 0) {
        return;
    }

    bool carry_out = false;
    for (unsigned int i = 0; i < count; i++) {
        bool sign = (result & sign_mask) != 0;
        carry_out = (result & 0x1ULL) != 0;
        result >>= 1;
        if (sign) {
            result |= sign_mask;
        }
        result &= mask;
    }

    write_sar_rm_operand(ctx, modrm, mem_addr, operand_size, result);
    update_flags_sar(ctx, result, operand_size, count, carry_out);
}

void decode_modrm_sar(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

    if (has_lock_prefix && mod == 3) {
        raise_ud_ctx(ctx);
    }
}

DecodedInstruction decode_sar_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    switch (inst.opcode) {
    case 0xD0:
    case 0xD2:
    case 0xC0:
        inst.operand_size = 8;
        decode_modrm_sar(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 7) {
            raise_ud_ctx(ctx);
        }
        if (inst.opcode == 0xC0) {
            if (offset >= code_size) {
                raise_gp_ctx(ctx, 0);
return inst;
            }
            inst.immediate = code[offset++];
            inst.imm_size = 1;
        }
        break;

    case 0xD1:
    case 0xD3:
    case 0xC1:
        decode_modrm_sar(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 7) {
            raise_ud_ctx(ctx);
        }
        if (inst.opcode == 0xC1) {
            if (offset >= code_size) {
                raise_gp_ctx(ctx, 0);
return inst;
            }
            inst.immediate = code[offset++];
            inst.imm_size = 1;
        }
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_sar_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    DecodedInstruction inst = *inst_ptr;
    uint8_t count = decode_sar_count(ctx, &inst);
    sar_rm(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.operand_size, count);
}

void execute_sar(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_sar_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_sar_with_decoded(ctx, &inst);
}

inline void execute_sar_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_sar_with_decoded(ctx, &dec->cached);
}
