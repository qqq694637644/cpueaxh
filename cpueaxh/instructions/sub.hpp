#pragma once

// instrusments/sub.hpp - SUB instruction implementation

// --- SUB execution helpers ---

// 2C ib - SUB AL, imm8
void sub_al_imm8(CPU_CONTEXT* ctx, uint8_t imm) {
    uint8_t dst = (uint8_t)(ctx->regs[REG_RAX] & 0xFF);
    uint16_t result = (uint16_t)dst - (uint16_t)imm;
    update_flags_sub8(ctx, dst, imm, result);
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFFULL) | (uint8_t)result;
}

// 2D iw - SUB AX, imm16
void sub_ax_imm16(CPU_CONTEXT* ctx, uint16_t imm) {
    uint16_t dst = (uint16_t)(ctx->regs[REG_RAX] & 0xFFFF);
    uint32_t result = (uint32_t)dst - (uint32_t)imm;
    update_flags_sub16(ctx, dst, imm, result);
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFFFFULL) | (uint16_t)result;
}

// 2D id - SUB EAX, imm32
void sub_eax_imm32(CPU_CONTEXT* ctx, uint32_t imm) {
    uint32_t dst = (uint32_t)(ctx->regs[REG_RAX] & 0xFFFFFFFF);
    uint64_t result = (uint64_t)dst - (uint64_t)imm;
    update_flags_sub32(ctx, dst, imm, result);
    // Writing to 32-bit register zero-extends to 64 bits
    ctx->regs[REG_RAX] = (uint32_t)result;
}

// REX.W + 2D id - SUB RAX, imm32 (sign-extended)
void sub_rax_imm32(CPU_CONTEXT* ctx, int32_t imm) {
    uint64_t dst = ctx->regs[REG_RAX];
    uint64_t src = (uint64_t)(int64_t)imm;
    uint64_t result = dst - src;
    update_flags_sub64(ctx, dst, src, result);
    ctx->regs[REG_RAX] = result;
}

// 80 /5 ib - SUB r/m8, imm8
void sub_rm8_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint8_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint8_t dst;
    if (mod == 3) {
        dst = get_reg8(ctx, rm);
    }
    else {
        dst = read_memory_byte(ctx, mem_addr);
    }

    uint16_t result = (uint16_t)dst - (uint16_t)imm;
    update_flags_sub8(ctx, dst, imm, result);

    if (mod == 3) {
        set_reg8(ctx, rm, (uint8_t)result);
    }
    else {
        write_memory_byte(ctx, mem_addr, (uint8_t)result);
    }
}

// 81 /5 iw - SUB r/m16, imm16
void sub_rm16_imm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint16_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint16_t dst;
    if (mod == 3) {
        dst = get_reg16(ctx, rm);
    }
    else {
        dst = read_memory_word(ctx, mem_addr);
    }

    uint32_t result = (uint32_t)dst - (uint32_t)imm;
    update_flags_sub16(ctx, dst, imm, result);

    if (mod == 3) {
        set_reg16(ctx, rm, (uint16_t)result);
    }
    else {
        write_memory_word(ctx, mem_addr, (uint16_t)result);
    }
}

// 81 /5 id - SUB r/m32, imm32
void sub_rm32_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint32_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint32_t dst;
    if (mod == 3) {
        dst = get_reg32(ctx, rm);
    }
    else {
        dst = read_memory_dword(ctx, mem_addr);
    }

    uint64_t result = (uint64_t)dst - (uint64_t)imm;
    update_flags_sub32(ctx, dst, imm, result);

    if (mod == 3) {
        set_reg32(ctx, rm, (uint32_t)result);
    }
    else {
        write_memory_dword(ctx, mem_addr, (uint32_t)result);
    }
}

// REX.W + 81 /5 id - SUB r/m64, imm32 (sign-extended)
void sub_rm64_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int32_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint64_t dst;
    if (mod == 3) {
        dst = get_reg64(ctx, rm);
    }
    else {
        dst = read_memory_qword(ctx, mem_addr);
    }

    uint64_t src = (uint64_t)(int64_t)imm;
    uint64_t result = dst - src;
    update_flags_sub64(ctx, dst, src, result);

    if (mod == 3) {
        set_reg64(ctx, rm, result);
    }
    else {
        write_memory_qword(ctx, mem_addr, result);
    }
}

// 83 /5 ib - SUB r/m16, imm8 (sign-extended)
void sub_rm16_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint16_t dst;
    if (mod == 3) {
        dst = get_reg16(ctx, rm);
    }
    else {
        dst = read_memory_word(ctx, mem_addr);
    }

    uint16_t src = (uint16_t)(int16_t)imm;
    uint32_t result = (uint32_t)dst - (uint32_t)src;
    update_flags_sub16(ctx, dst, src, result);

    if (mod == 3) {
        set_reg16(ctx, rm, (uint16_t)result);
    }
    else {
        write_memory_word(ctx, mem_addr, (uint16_t)result);
    }
}

// 83 /5 ib - SUB r/m32, imm8 (sign-extended)
void sub_rm32_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint32_t dst;
    if (mod == 3) {
        dst = get_reg32(ctx, rm);
    }
    else {
        dst = read_memory_dword(ctx, mem_addr);
    }

    uint32_t src = (uint32_t)(int32_t)imm;
    uint64_t result = (uint64_t)dst - (uint64_t)src;
    update_flags_sub32(ctx, dst, src, result);

    if (mod == 3) {
        set_reg32(ctx, rm, (uint32_t)result);
    }
    else {
        write_memory_dword(ctx, mem_addr, (uint32_t)result);
    }
}

// REX.W + 83 /5 ib - SUB r/m64, imm8 (sign-extended)
void sub_rm64_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint64_t dst;
    if (mod == 3) {
        dst = get_reg64(ctx, rm);
    }
    else {
        dst = read_memory_qword(ctx, mem_addr);
    }

    uint64_t src = (uint64_t)(int64_t)imm;
    uint64_t result = dst - src;
    update_flags_sub64(ctx, dst, src, result);

    if (mod == 3) {
        set_reg64(ctx, rm, result);
    }
    else {
        write_memory_qword(ctx, mem_addr, result);
    }
}

// 28 /r - SUB r/m8, r8
void sub_rm8_r8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint8_t src = get_reg8(ctx, reg);
    uint8_t dst;
    if (mod == 3) {
        dst = get_reg8(ctx, rm);
    }
    else {
        dst = read_memory_byte(ctx, mem_addr);
    }

    uint16_t result = (uint16_t)dst - (uint16_t)src;
    update_flags_sub8(ctx, dst, src, result);

    if (mod == 3) {
        set_reg8(ctx, rm, (uint8_t)result);
    }
    else {
        write_memory_byte(ctx, mem_addr, (uint8_t)result);
    }
}

// 29 /r - SUB r/m16, r16
void sub_rm16_r16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint16_t src = get_reg16(ctx, reg);
    uint16_t dst;
    if (mod == 3) {
        dst = get_reg16(ctx, rm);
    }
    else {
        dst = read_memory_word(ctx, mem_addr);
    }

    uint32_t result = (uint32_t)dst - (uint32_t)src;
    update_flags_sub16(ctx, dst, src, result);

    if (mod == 3) {
        set_reg16(ctx, rm, (uint16_t)result);
    }
    else {
        write_memory_word(ctx, mem_addr, (uint16_t)result);
    }
}

// 29 /r - SUB r/m32, r32
void sub_rm32_r32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint32_t src = get_reg32(ctx, reg);
    uint32_t dst;
    if (mod == 3) {
        dst = get_reg32(ctx, rm);
    }
    else {
        dst = read_memory_dword(ctx, mem_addr);
    }

    uint64_t result = (uint64_t)dst - (uint64_t)src;
    update_flags_sub32(ctx, dst, src, result);

    if (mod == 3) {
        set_reg32(ctx, rm, (uint32_t)result);
    }
    else {
        write_memory_dword(ctx, mem_addr, (uint32_t)result);
    }
}

// REX.W + 29 /r - SUB r/m64, r64
void sub_rm64_r64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint64_t src = get_reg64(ctx, reg);
    uint64_t dst;
    if (mod == 3) {
        dst = get_reg64(ctx, rm);
    }
    else {
        dst = read_memory_qword(ctx, mem_addr);
    }

    uint64_t result = dst - src;
    update_flags_sub64(ctx, dst, src, result);

    if (mod == 3) {
        set_reg64(ctx, rm, result);
    }
    else {
        write_memory_qword(ctx, mem_addr, result);
    }
}

// 2A /r - SUB r8, r/m8
void sub_r8_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint8_t dst = get_reg8(ctx, reg);
    uint8_t src;
    if (mod == 3) {
        src = get_reg8(ctx, rm);
    }
    else {
        src = read_memory_byte(ctx, mem_addr);
    }

    uint16_t result = (uint16_t)dst - (uint16_t)src;
    update_flags_sub8(ctx, dst, src, result);

    set_reg8(ctx, reg, (uint8_t)result);
}

// 2B /r - SUB r16, r/m16
void sub_r16_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint16_t dst = get_reg16(ctx, reg);
    uint16_t src;
    if (mod == 3) {
        src = get_reg16(ctx, rm);
    }
    else {
        src = read_memory_word(ctx, mem_addr);
    }

    uint32_t result = (uint32_t)dst - (uint32_t)src;
    update_flags_sub16(ctx, dst, src, result);

    set_reg16(ctx, reg, (uint16_t)result);
}

// 2B /r - SUB r32, r/m32
void sub_r32_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint32_t dst = get_reg32(ctx, reg);
    uint32_t src;
    if (mod == 3) {
        src = get_reg32(ctx, rm);
    }
    else {
        src = read_memory_dword(ctx, mem_addr);
    }

    uint64_t result = (uint64_t)dst - (uint64_t)src;
    update_flags_sub32(ctx, dst, src, result);

    set_reg32(ctx, reg, (uint32_t)result);
}

// REX.W + 2B /r - SUB r64, r/m64
void sub_r64_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint64_t dst = get_reg64(ctx, reg);
    uint64_t src;
    if (mod == 3) {
        src = get_reg64(ctx, rm);
    }
    else {
        src = read_memory_qword(ctx, mem_addr);
    }

    uint64_t result = dst - src;
    update_flags_sub64(ctx, dst, src, result);

    set_reg64(ctx, reg, result);
}

// --- Helper: decode ModR/M for SUB ---

void decode_modrm_sub(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
    if (*offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return;
    }
    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    uint8_t mod = (inst->modrm >> 6) & 0x03;
    uint8_t rm = inst->modrm & 0x07;

    // SIB byte present when rm==4 and mod!=3 (not in 16-bit addressing)
    if (mod != 3 && rm == 4 && inst->address_size != 16) {
        if (*offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }
        inst->has_sib = true;
        inst->sib = code[(*offset)++];
    }

    // Determine displacement size
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

    // LOCK prefix requires memory destination
    if (has_lock_prefix && mod == 3) {
        raise_ud_ctx(ctx);
    }
}

// --- SUB instruction decoder ---

DecodedInstruction decode_sub_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    // Decode prefixes
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

    // Determine operand size
    inst.operand_size = 32;
    if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }

    // Determine address size
    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    switch (inst.opcode) {
    // 2C ib - SUB AL, imm8
    case 0x2C:
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

    // 2D iw/id - SUB AX, imm16 / SUB EAX, imm32 / REX.W + SUB RAX, imm32
    case 0x2D:
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

    // 80 /5 ib - SUB r/m8, imm8
    case 0x80:
        inst.operand_size = 8;
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 5) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // 81 /5 iw/id - SUB r/m16, imm16 / SUB r/m32, imm32 / REX.W + SUB r/m64, imm32
    case 0x81:
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 5) {
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

    // 83 /5 ib - SUB r/m16, imm8 / SUB r/m32, imm8 / REX.W + SUB r/m64, imm8
    case 0x83:
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 5) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // 28 /r - SUB r/m8, r8
    case 0x28:
        inst.operand_size = 8;
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 29 /r - SUB r/m16, r16 / SUB r/m32, r32 / REX.W + SUB r/m64, r64
    case 0x29:
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 2A /r - SUB r8, r/m8
    case 0x2A:
        inst.operand_size = 8;
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 2B /r - SUB r16, r/m16 / SUB r32, r/m32 / REX.W + SUB r64, r/m64
    case 0x2B:
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sub(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

// --- SUB instruction executor ---

inline void execute_sub_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    // 2C ib - SUB AL, imm8
    case 0x2C:
        sub_al_imm8(ctx, (uint8_t)inst.immediate);
        break;

    // 2D iw/id - SUB AX, imm16 / SUB EAX, imm32 / REX.W + SUB RAX, imm32 (sign-extended)
    case 0x2D:
        if (ctx->rex_w) {
            sub_rax_imm32(ctx, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            sub_ax_imm16(ctx, (uint16_t)inst.immediate);
        }
        else {
            sub_eax_imm32(ctx, (uint32_t)inst.immediate);
        }
        break;

    // 80 /5 ib - SUB r/m8, imm8
    case 0x80:
        sub_rm8_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint8_t)inst.immediate);
        break;

    // 81 /5 iw/id - SUB r/m16, imm16 / SUB r/m32, imm32 / REX.W + SUB r/m64, imm32 (sign-extended)
    case 0x81:
        if (ctx->rex_w) {
            sub_rm64_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            sub_rm16_imm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint16_t)inst.immediate);
        }
        else {
            sub_rm32_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint32_t)inst.immediate);
        }
        break;

    // 83 /5 ib - SUB r/m16, imm8 / SUB r/m32, imm8 / REX.W + SUB r/m64, imm8 (sign-extended)
    case 0x83:
        if (ctx->rex_w) {
            sub_rm64_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            sub_rm16_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else {
            sub_rm32_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        break;

    // 28 /r - SUB r/m8, r8
    case 0x28:
        sub_rm8_r8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    // 29 /r - SUB r/m16, r16 / SUB r/m32, r32 / REX.W + SUB r/m64, r64
    case 0x29:
        if (ctx->rex_w) {
            sub_rm64_r64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            sub_rm16_r16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            sub_rm32_r32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;

    // 2A /r - SUB r8, r/m8
    case 0x2A:
        sub_r8_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    // 2B /r - SUB r16, r/m16 / SUB r32, r/m32 / REX.W + SUB r64, r/m64
    case 0x2B:
        if (ctx->rex_w) {
            sub_r64_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            sub_r16_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            sub_r32_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    }
}

void execute_sub(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_sub_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_sub_with_decoded(ctx, &inst);
}

inline void execute_sub_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_sub_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_sub_with_decoded(ctx, &live);
}
