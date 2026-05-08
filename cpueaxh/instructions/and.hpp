// instrusments/and.hpp - AND instruction implementation

// --- AND execution helpers ---

// 24 ib - AND AL, imm8
void and_al_imm8(CPU_CONTEXT* ctx, uint8_t imm) {
    uint8_t dst = (uint8_t)(ctx->regs[REG_RAX] & 0xFF);
    uint8_t result = dst & imm;
    update_flags_and8(ctx, result);
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFFULL) | result;
}

// 25 iw - AND AX, imm16
void and_ax_imm16(CPU_CONTEXT* ctx, uint16_t imm) {
    uint16_t dst = (uint16_t)(ctx->regs[REG_RAX] & 0xFFFF);
    uint16_t result = dst & imm;
    update_flags_and16(ctx, result);
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFFFFULL) | result;
}

// 25 id - AND EAX, imm32
void and_eax_imm32(CPU_CONTEXT* ctx, uint32_t imm) {
    uint32_t dst = (uint32_t)(ctx->regs[REG_RAX] & 0xFFFFFFFF);
    uint32_t result = dst & imm;
    update_flags_and32(ctx, result);
    // 32-bit write zero-extends to 64 bits
    ctx->regs[REG_RAX] = result;
}

// REX.W + 25 id - AND RAX, imm32 (sign-extended to 64 bits)
void and_rax_imm32(CPU_CONTEXT* ctx, int32_t imm) {
    uint64_t dst = ctx->regs[REG_RAX];
    uint64_t src = (uint64_t)(int64_t)imm;
    uint64_t result = dst & src;
    update_flags_and64(ctx, result);
    ctx->regs[REG_RAX] = result;
}

// 80 /4 ib - AND r/m8, imm8
void and_rm8_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint8_t imm) {
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

    uint8_t result = dst & imm;
    update_flags_and8(ctx, result);

    if (mod == 3) {
        set_reg8(ctx, rm, result);
    }
    else {
        write_memory_byte(ctx, mem_addr, result);
    }
}

// 81 /4 iw - AND r/m16, imm16
void and_rm16_imm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint16_t imm) {
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

    uint16_t result = dst & imm;
    update_flags_and16(ctx, result);

    if (mod == 3) {
        set_reg16(ctx, rm, result);
    }
    else {
        write_memory_word(ctx, mem_addr, result);
    }
}

// 81 /4 id - AND r/m32, imm32
void and_rm32_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint32_t imm) {
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

    uint32_t result = dst & imm;
    update_flags_and32(ctx, result);

    if (mod == 3) {
        set_reg32(ctx, rm, result);
    }
    else {
        write_memory_dword(ctx, mem_addr, result);
    }
}

// REX.W + 81 /4 id - AND r/m64, imm32 (sign-extended to 64 bits)
void and_rm64_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int32_t imm) {
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
    uint64_t result = dst & src;
    update_flags_and64(ctx, result);

    if (mod == 3) {
        set_reg64(ctx, rm, result);
    }
    else {
        write_memory_qword(ctx, mem_addr, result);
    }
}

// 83 /4 ib - AND r/m16, imm8 (sign-extended)
void and_rm16_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
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
    uint16_t result = dst & src;
    update_flags_and16(ctx, result);

    if (mod == 3) {
        set_reg16(ctx, rm, result);
    }
    else {
        write_memory_word(ctx, mem_addr, result);
    }
}

// 83 /4 ib - AND r/m32, imm8 (sign-extended)
void and_rm32_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
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
    uint32_t result = dst & src;
    update_flags_and32(ctx, result);

    if (mod == 3) {
        set_reg32(ctx, rm, result);
    }
    else {
        write_memory_dword(ctx, mem_addr, result);
    }
}

// REX.W + 83 /4 ib - AND r/m64, imm8 (sign-extended)
void and_rm64_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
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
    uint64_t result = dst & src;
    update_flags_and64(ctx, result);

    if (mod == 3) {
        set_reg64(ctx, rm, result);
    }
    else {
        write_memory_qword(ctx, mem_addr, result);
    }
}

// 20 /r - AND r/m8, r8
void and_rm8_r8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint8_t result = dst & src;
    update_flags_and8(ctx, result);

    if (mod == 3) {
        set_reg8(ctx, rm, result);
    }
    else {
        write_memory_byte(ctx, mem_addr, result);
    }
}

// 21 /r - AND r/m16, r16
void and_rm16_r16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint16_t result = dst & src;
    update_flags_and16(ctx, result);

    if (mod == 3) {
        set_reg16(ctx, rm, result);
    }
    else {
        write_memory_word(ctx, mem_addr, result);
    }
}

// 21 /r - AND r/m32, r32
void and_rm32_r32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint32_t result = dst & src;
    update_flags_and32(ctx, result);

    if (mod == 3) {
        set_reg32(ctx, rm, result);
    }
    else {
        write_memory_dword(ctx, mem_addr, result);
    }
}

// REX.W + 21 /r - AND r/m64, r64
void and_rm64_r64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint64_t result = dst & src;
    update_flags_and64(ctx, result);

    if (mod == 3) {
        set_reg64(ctx, rm, result);
    }
    else {
        write_memory_qword(ctx, mem_addr, result);
    }
}

// 22 /r - AND r8, r/m8
void and_r8_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint8_t result = dst & src;
    update_flags_and8(ctx, result);
    set_reg8(ctx, reg, result);
}

// 23 /r - AND r16, r/m16
void and_r16_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint16_t result = dst & src;
    update_flags_and16(ctx, result);
    set_reg16(ctx, reg, result);
}

// 23 /r - AND r32, r/m32
void and_r32_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint32_t result = dst & src;
    update_flags_and32(ctx, result);
    set_reg32(ctx, reg, result);
}

// REX.W + 23 /r - AND r64, r/m64
void and_r64_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
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

    uint64_t result = dst & src;
    update_flags_and64(ctx, result);
    set_reg64(ctx, reg, result);
}

// --- Helper: decode ModR/M for AND ---

void decode_modrm_and(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

// --- AND instruction decoder ---

DecodedInstruction decode_and_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    // 24 ib - AND AL, imm8
    case 0x24:
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

    // 25 iw/id - AND AX, imm16 / AND EAX, imm32 / REX.W + AND RAX, imm32
    case 0x25:
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

    // 80 /4 ib - AND r/m8, imm8
    case 0x80:
        inst.operand_size = 8;
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 4) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // 81 /4 iw/id - AND r/m16, imm16 / AND r/m32, imm32 / REX.W + AND r/m64, imm32
    case 0x81:
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 4) {
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

    // 83 /4 ib - AND r/m16, imm8 / AND r/m32, imm8 / REX.W + AND r/m64, imm8
    case 0x83:
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 4) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // 20 /r - AND r/m8, r8
    case 0x20:
        inst.operand_size = 8;
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 21 /r - AND r/m16, r16 / AND r/m32, r32 / REX.W + AND r/m64, r64
    case 0x21:
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 22 /r - AND r8, r/m8
    case 0x22:
        inst.operand_size = 8;
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 23 /r - AND r16, r/m16 / AND r32, r/m32 / REX.W + AND r64, r/m64
    case 0x23:
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_and(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

// --- AND instruction executor ---

inline void execute_and_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    // 24 ib - AND AL, imm8
    case 0x24:
        and_al_imm8(ctx, (uint8_t)inst.immediate);
        break;

    // 25 iw/id - AND AX, imm16 / AND EAX, imm32 / REX.W + AND RAX, imm32
    case 0x25:
        if (ctx->rex_w) {
            and_rax_imm32(ctx, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            and_ax_imm16(ctx, (uint16_t)inst.immediate);
        }
        else {
            and_eax_imm32(ctx, (uint32_t)inst.immediate);
        }
        break;

    // 80 /4 ib - AND r/m8, imm8
    case 0x80:
        and_rm8_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint8_t)inst.immediate);
        break;

    // 81 /4 iw/id - AND r/m16, imm16 / AND r/m32, imm32 / REX.W + AND r/m64, imm32
    case 0x81:
        if (ctx->rex_w) {
            and_rm64_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            and_rm16_imm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint16_t)inst.immediate);
        }
        else {
            and_rm32_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint32_t)inst.immediate);
        }
        break;

    // 83 /4 ib - AND r/m16, imm8 / AND r/m32, imm8 / REX.W + AND r/m64, imm8
    case 0x83:
        if (ctx->rex_w) {
            and_rm64_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            and_rm16_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else {
            and_rm32_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        break;

    // 20 /r - AND r/m8, r8
    case 0x20:
        and_rm8_r8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    // 21 /r - AND r/m16, r16 / AND r/m32, r32 / REX.W + AND r/m64, r64
    case 0x21:
        if (ctx->rex_w) {
            and_rm64_r64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            and_rm16_r16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            and_rm32_r32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;

    // 22 /r - AND r8, r/m8
    case 0x22:
        and_r8_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    // 23 /r - AND r16, r/m16 / AND r32, r/m32 / REX.W + AND r64, r/m64
    case 0x23:
        if (ctx->rex_w) {
            and_r64_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            and_r16_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            and_r32_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    }
}

void execute_and(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_and_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_and_with_decoded(ctx, &inst);
}

inline void execute_and_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_and_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_and_with_decoded(ctx, &live);
}
