#pragma once

// instrusments/adc.hpp - ADC instruction implementation

static uint8_t adc_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    uint8_t rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static uint8_t adc_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    uint8_t reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

uint8_t adc_value8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t src) {
    uint8_t carry_in = (ctx->rflags & RFLAGS_CF) != 0 ? 1 : 0;
    uint16_t result = (uint16_t)dst + (uint16_t)src + (uint16_t)carry_in;
    update_flags_adc8(ctx, dst, src, carry_in, result);
    return (uint8_t)result;
}

uint8_t adc_read_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        return get_reg8(ctx, rm);
    }
    return read_memory_byte(ctx, mem_addr);
}

void adc_write_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, uint8_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        set_reg8(ctx, rm, value);
    }
    else {
        write_memory_byte(ctx, mem_addr, value);
    }
}

uint8_t adc_read_reg8(CPU_CONTEXT* ctx, uint8_t modrm) {
    return get_reg8(ctx, adc_reg_index(ctx, modrm));
}

void adc_write_reg8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t value) {
    set_reg8(ctx, adc_reg_index(ctx, modrm), value);
}

uint16_t adc_value16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t src) {
    uint8_t carry_in = (ctx->rflags & RFLAGS_CF) != 0 ? 1 : 0;
    uint32_t result = (uint32_t)dst + (uint32_t)src + (uint32_t)carry_in;
    update_flags_adc16(ctx, dst, src, carry_in, result);
    return (uint16_t)result;
}

uint16_t adc_read_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        return get_reg16(ctx, rm);
    }
    return read_memory_word(ctx, mem_addr);
}

void adc_write_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, uint16_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        set_reg16(ctx, rm, value);
    }
    else {
        write_memory_word(ctx, mem_addr, value);
    }
}

uint16_t adc_read_reg16(CPU_CONTEXT* ctx, uint8_t modrm) {
    return get_reg16(ctx, adc_reg_index(ctx, modrm));
}

void adc_write_reg16(CPU_CONTEXT* ctx, uint8_t modrm, uint16_t value) {
    set_reg16(ctx, adc_reg_index(ctx, modrm), value);
}

uint32_t adc_value32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t src) {
    uint8_t carry_in = (ctx->rflags & RFLAGS_CF) != 0 ? 1 : 0;
    uint64_t result = (uint64_t)dst + (uint64_t)src + (uint64_t)carry_in;
    update_flags_adc32(ctx, dst, src, carry_in, result);
    return (uint32_t)result;
}

uint32_t adc_read_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        return get_reg32(ctx, rm);
    }
    return read_memory_dword(ctx, mem_addr);
}

void adc_write_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, uint32_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        set_reg32(ctx, rm, value);
    }
    else {
        write_memory_dword(ctx, mem_addr, value);
    }
}

uint32_t adc_read_reg32(CPU_CONTEXT* ctx, uint8_t modrm) {
    return get_reg32(ctx, adc_reg_index(ctx, modrm));
}

void adc_write_reg32(CPU_CONTEXT* ctx, uint8_t modrm, uint32_t value) {
    set_reg32(ctx, adc_reg_index(ctx, modrm), value);
}

uint64_t adc_value64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t src) {
    uint8_t carry_in = (ctx->rflags & RFLAGS_CF) != 0 ? 1 : 0;
    uint64_t result = dst + src + (uint64_t)carry_in;
    update_flags_adc64(ctx, dst, src, carry_in, result);
    return (uint64_t)result;
}

uint64_t adc_read_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        return get_reg64(ctx, rm);
    }
    return read_memory_qword(ctx, mem_addr);
}

void adc_write_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, uint64_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = adc_rm_index(ctx, modrm);
    if (mod == 3) {
        set_reg64(ctx, rm, value);
    }
    else {
        write_memory_qword(ctx, mem_addr, value);
    }
}

uint64_t adc_read_reg64(CPU_CONTEXT* ctx, uint8_t modrm) {
    return get_reg64(ctx, adc_reg_index(ctx, modrm));
}

void adc_write_reg64(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t value) {
    set_reg64(ctx, adc_reg_index(ctx, modrm), value);
}

void adc_al_imm8(CPU_CONTEXT* ctx, uint8_t imm) {
    uint8_t dst = (uint8_t)(ctx->regs[REG_RAX] & 0xFF);
    uint8_t result = adc_value8(ctx, dst, (uint8_t)imm);
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFFULL) | result;
}

void adc_ax_imm16(CPU_CONTEXT* ctx, uint16_t imm) {
    uint16_t dst = (uint16_t)(ctx->regs[REG_RAX] & 0xFFFF);
    uint16_t result = adc_value16(ctx, dst, (uint16_t)imm);
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFFFFULL) | result;
}

void adc_eax_imm32(CPU_CONTEXT* ctx, uint32_t imm) {
    uint32_t dst = (uint32_t)(ctx->regs[REG_RAX] & 0xFFFFFFFFULL);
    uint32_t result = adc_value32(ctx, dst, (uint32_t)imm);
    ctx->regs[REG_RAX] = result;
}

void adc_rax_imm32(CPU_CONTEXT* ctx, int32_t imm) {
    uint64_t dst = ctx->regs[REG_RAX];
    uint64_t result = adc_value64(ctx, dst, (uint64_t)(uint64_t)(int64_t)imm);
    ctx->regs[REG_RAX] = result;
}

void adc_rm8_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint8_t imm) {
    uint8_t dst = adc_read_rm8(ctx, modrm, mem_addr);
    uint8_t result = adc_value8(ctx, dst, (uint8_t)(uint8_t)imm);
    adc_write_rm8(ctx, modrm, mem_addr, result);
}

void adc_rm16_imm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint16_t imm) {
    uint16_t dst = adc_read_rm16(ctx, modrm, mem_addr);
    uint16_t result = adc_value16(ctx, dst, (uint16_t)(uint16_t)imm);
    adc_write_rm16(ctx, modrm, mem_addr, result);
}

void adc_rm32_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint32_t imm) {
    uint32_t dst = adc_read_rm32(ctx, modrm, mem_addr);
    uint32_t result = adc_value32(ctx, dst, (uint32_t)(uint32_t)imm);
    adc_write_rm32(ctx, modrm, mem_addr, result);
}

void adc_rm64_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int32_t imm) {
    uint64_t dst = adc_read_rm64(ctx, modrm, mem_addr);
    uint64_t result = adc_value64(ctx, dst, (uint64_t)(uint64_t)(int64_t)imm);
    adc_write_rm64(ctx, modrm, mem_addr, result);
}

void adc_rm16_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    uint16_t dst = adc_read_rm16(ctx, modrm, mem_addr);
    uint16_t result = adc_value16(ctx, dst, (uint16_t)(uint16_t)(int16_t)imm);
    adc_write_rm16(ctx, modrm, mem_addr, result);
}

void adc_rm32_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    uint32_t dst = adc_read_rm32(ctx, modrm, mem_addr);
    uint32_t result = adc_value32(ctx, dst, (uint32_t)(uint32_t)(int32_t)imm);
    adc_write_rm32(ctx, modrm, mem_addr, result);
}

void adc_rm64_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    uint64_t dst = adc_read_rm64(ctx, modrm, mem_addr);
    uint64_t result = adc_value64(ctx, dst, (uint64_t)(uint64_t)(int64_t)imm);
    adc_write_rm64(ctx, modrm, mem_addr, result);
}

void adc_rm8_r8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t dst = adc_read_rm8(ctx, modrm, mem_addr);
    uint8_t src = adc_read_reg8(ctx, modrm);
    uint8_t result = adc_value8(ctx, dst, src);
    adc_write_rm8(ctx, modrm, mem_addr, result);
}

void adc_r8_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t dst = adc_read_reg8(ctx, modrm);
    uint8_t src = adc_read_rm8(ctx, modrm, mem_addr);
    uint8_t result = adc_value8(ctx, dst, src);
    adc_write_reg8(ctx, modrm, result);
}

void adc_rm16_r16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint16_t dst = adc_read_rm16(ctx, modrm, mem_addr);
    uint16_t src = adc_read_reg16(ctx, modrm);
    uint16_t result = adc_value16(ctx, dst, src);
    adc_write_rm16(ctx, modrm, mem_addr, result);
}

void adc_r16_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint16_t dst = adc_read_reg16(ctx, modrm);
    uint16_t src = adc_read_rm16(ctx, modrm, mem_addr);
    uint16_t result = adc_value16(ctx, dst, src);
    adc_write_reg16(ctx, modrm, result);
}

void adc_rm32_r32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint32_t dst = adc_read_rm32(ctx, modrm, mem_addr);
    uint32_t src = adc_read_reg32(ctx, modrm);
    uint32_t result = adc_value32(ctx, dst, src);
    adc_write_rm32(ctx, modrm, mem_addr, result);
}

void adc_r32_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint32_t dst = adc_read_reg32(ctx, modrm);
    uint32_t src = adc_read_rm32(ctx, modrm, mem_addr);
    uint32_t result = adc_value32(ctx, dst, src);
    adc_write_reg32(ctx, modrm, result);
}

void adc_rm64_r64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint64_t dst = adc_read_rm64(ctx, modrm, mem_addr);
    uint64_t src = adc_read_reg64(ctx, modrm);
    uint64_t result = adc_value64(ctx, dst, src);
    adc_write_rm64(ctx, modrm, mem_addr, result);
}

void adc_r64_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint64_t dst = adc_read_reg64(ctx, modrm);
    uint64_t src = adc_read_rm64(ctx, modrm, mem_addr);
    uint64_t result = adc_value64(ctx, dst, src);
    adc_write_reg64(ctx, modrm, result);
}

void decode_modrm_adc(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_adc_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    case 0x14:
        inst.operand_size = 8;
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    case 0x15:
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        if (ctx->rex_w) {
            inst.imm_size = 4;
        }
        else if (ctx->operand_size_override) {
            inst.imm_size = 2;
        }
        else {
            inst.imm_size = 4;
        }
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = 0;
        for (int i = 0; i < inst.imm_size; i++) {
            inst.immediate |= ((uint64_t)code[offset++]) << (i * 8);
        }
        break;

    case 0x80:
        inst.operand_size = 8;
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 2) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    case 0x81:
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 2) {
            raise_ud_ctx(ctx);
        }
        if (ctx->rex_w) {
            inst.imm_size = 4;
        }
        else if (ctx->operand_size_override) {
            inst.imm_size = 2;
        }
        else {
            inst.imm_size = 4;
        }
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = 0;
        for (int i = 0; i < inst.imm_size; i++) {
            inst.immediate |= ((uint64_t)code[offset++]) << (i * 8);
        }
        break;

    case 0x83:
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 2) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    case 0x10:
        inst.operand_size = 8;
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    case 0x11:
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    case 0x12:
        inst.operand_size = 8;
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    case 0x13:
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_adc(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_adc_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    case 0x14:
        adc_al_imm8(ctx, (uint8_t)inst.immediate);
        break;
    case 0x15:
        if (inst.operand_size == 64) {
            adc_rax_imm32(ctx, (int32_t)inst.immediate);
        }
        else if (inst.operand_size == 16) {
            adc_ax_imm16(ctx, (uint16_t)inst.immediate);
        }
        else {
            adc_eax_imm32(ctx, (uint32_t)inst.immediate);
        }
        break;
    case 0x80:
        adc_rm8_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint8_t)inst.immediate);
        break;
    case 0x81:
        if (inst.operand_size == 64) {
            adc_rm64_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int32_t)inst.immediate);
        }
        else if (inst.operand_size == 16) {
            adc_rm16_imm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint16_t)inst.immediate);
        }
        else {
            adc_rm32_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint32_t)inst.immediate);
        }
        break;
    case 0x83:
        if (inst.operand_size == 64) {
            adc_rm64_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else if (inst.operand_size == 16) {
            adc_rm16_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else {
            adc_rm32_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        break;
    case 0x10:
        adc_rm8_r8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;
    case 0x11:
        if (inst.operand_size == 64) {
            adc_rm64_r64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (inst.operand_size == 16) {
            adc_rm16_r16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            adc_rm32_r32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    case 0x12:
        adc_r8_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;
    case 0x13:
        if (inst.operand_size == 64) {
            adc_r64_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (inst.operand_size == 16) {
            adc_r16_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            adc_r32_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    default:
        raise_ud_ctx(ctx);
    }
}

void execute_adc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_adc_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_adc_with_decoded(ctx, &inst);
}

inline void execute_adc_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_adc_with_decoded(ctx, &dec->cached);
}
