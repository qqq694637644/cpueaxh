// instrusments/xor.hpp - XOR instruction implementation

// --- XOR internal helpers ---

int decode_xor_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_xor_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

uint64_t read_xor_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_xor_rm_index(ctx, modrm);

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

void write_xor_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size, uint64_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_xor_rm_index(ctx, modrm);

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

uint64_t read_xor_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size) {
    int reg = decode_xor_reg_index(ctx, modrm);

    switch (operand_size) {
    case 8:  return get_reg8(ctx, reg);
    case 16: return get_reg16(ctx, reg);
    case 32: return get_reg32(ctx, reg);
    case 64: return get_reg64(ctx, reg);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_xor_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size, uint64_t value) {
    int reg = decode_xor_reg_index(ctx, modrm);

    switch (operand_size) {
    case 8:  set_reg8(ctx, reg, (uint8_t)value); break;
    case 16: set_reg16(ctx, reg, (uint16_t)value); break;
    case 32: set_reg32(ctx, reg, (uint32_t)value); break;
    case 64: set_reg64(ctx, reg, value); break;
    default: raise_ud_ctx(ctx);
    }
}

void update_xor_logic_flags(CPU_CONTEXT* ctx, int operand_size, uint64_t result) {
    switch (operand_size) {
    case 8:  update_flags_xor8(ctx, (uint8_t)result); break;
    case 16: update_flags_xor16(ctx, (uint16_t)result); break;
    case 32: update_flags_xor32(ctx, (uint32_t)result); break;
    case 64: update_flags_xor64(ctx, result); break;
    default: raise_ud_ctx(ctx);
    }
}

void xor_rm_imm(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size, uint64_t imm, bool has_lock_prefix) {
    if (has_lock_prefix && ((modrm >> 6) & 0x03) != 3) {
        uint64_t old_value = 0;
        uint64_t new_value = 0;
        if (!cpu_atomic_xor_memory(ctx, mem_addr, operand_size, imm, &old_value, &new_value)) {
            return;
        }
        update_xor_logic_flags(ctx, operand_size, new_value);
        return;
    }
    uint64_t dst = read_xor_rm_operand(ctx, modrm, mem_addr, operand_size);
    uint64_t result = dst ^ imm;
    update_xor_logic_flags(ctx, operand_size, result);
    write_xor_rm_operand(ctx, modrm, mem_addr, operand_size, result);
}

void xor_rm_r(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size, bool has_lock_prefix) {
    uint64_t src = read_xor_reg_operand(ctx, modrm, operand_size);
    if (has_lock_prefix && ((modrm >> 6) & 0x03) != 3) {
        uint64_t old_value = 0;
        uint64_t new_value = 0;
        if (!cpu_atomic_xor_memory(ctx, mem_addr, operand_size, src, &old_value, &new_value)) {
            return;
        }
        update_xor_logic_flags(ctx, operand_size, new_value);
        return;
    }
    uint64_t dst = read_xor_rm_operand(ctx, modrm, mem_addr, operand_size);
    uint64_t result = dst ^ src;
    update_xor_logic_flags(ctx, operand_size, result);
    write_xor_rm_operand(ctx, modrm, mem_addr, operand_size, result);
}

void xor_r_rm(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint64_t dst = read_xor_reg_operand(ctx, modrm, operand_size);
    uint64_t src = read_xor_rm_operand(ctx, modrm, mem_addr, operand_size);
    uint64_t result = dst ^ src;
    update_xor_logic_flags(ctx, operand_size, result);
    write_xor_reg_operand(ctx, modrm, operand_size, result);
}

// --- XOR execution helpers ---

// 34 ib - XOR AL, imm8
void xor_al_imm8(CPU_CONTEXT* ctx, uint8_t imm) {
    uint8_t result = get_reg8(ctx, REG_RAX) ^ imm;
    update_flags_xor8(ctx, result);
    set_reg8(ctx, REG_RAX, result);
}

// 35 iw - XOR AX, imm16
void xor_ax_imm16(CPU_CONTEXT* ctx, uint16_t imm) {
    uint16_t result = get_reg16(ctx, REG_RAX) ^ imm;
    update_flags_xor16(ctx, result);
    set_reg16(ctx, REG_RAX, result);
}

// 35 id - XOR EAX, imm32
void xor_eax_imm32(CPU_CONTEXT* ctx, uint32_t imm) {
    uint32_t result = get_reg32(ctx, REG_RAX) ^ imm;
    update_flags_xor32(ctx, result);
    set_reg32(ctx, REG_RAX, result);
}

// REX.W + 35 id - XOR RAX, imm32 (sign-extended to 64 bits)
void xor_rax_imm32(CPU_CONTEXT* ctx, int32_t imm) {
    uint64_t result = get_reg64(ctx, REG_RAX) ^ (uint64_t)(int64_t)imm;
    update_flags_xor64(ctx, result);
    set_reg64(ctx, REG_RAX, result);
}

// 80 /6 ib - XOR r/m8, imm8
void xor_rm8_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint8_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 8, imm, has_lock_prefix);
}

// 81 /6 iw - XOR r/m16, imm16
void xor_rm16_imm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint16_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 16, imm, has_lock_prefix);
}

// 81 /6 id - XOR r/m32, imm32
void xor_rm32_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint32_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 32, imm, has_lock_prefix);
}

// REX.W + 81 /6 id - XOR r/m64, imm32 (sign-extended to 64 bits)
void xor_rm64_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int32_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 64, (uint64_t)(int64_t)imm, has_lock_prefix);
}

// 83 /6 ib - XOR r/m16, imm8 (sign-extended)
void xor_rm16_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 16, (uint16_t)(int16_t)imm, has_lock_prefix);
}

// 83 /6 ib - XOR r/m32, imm8 (sign-extended)
void xor_rm32_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 32, (uint32_t)(int32_t)imm, has_lock_prefix);
}

// REX.W + 83 /6 ib - XOR r/m64, imm8 (sign-extended)
void xor_rm64_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_imm(ctx, modrm, mem_addr, 64, (uint64_t)(int64_t)imm, has_lock_prefix);
}

// 30 /r - XOR r/m8, r8
void xor_rm8_r8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_r(ctx, modrm, mem_addr, 8, has_lock_prefix);
}

// 31 /r - XOR r/m16, r16
void xor_rm16_r16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_r(ctx, modrm, mem_addr, 16, has_lock_prefix);
}

// 31 /r - XOR r/m32, r32
void xor_rm32_r32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_r(ctx, modrm, mem_addr, 32, has_lock_prefix);
}

// REX.W + 31 /r - XOR r/m64, r64
void xor_rm64_r64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, bool has_lock_prefix) {
    (void)sib;
    (void)disp;
    xor_rm_r(ctx, modrm, mem_addr, 64, has_lock_prefix);
}

// 32 /r - XOR r8, r/m8
void xor_r8_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    xor_r_rm(ctx, modrm, mem_addr, 8);
}

// 33 /r - XOR r16, r/m16
void xor_r16_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    xor_r_rm(ctx, modrm, mem_addr, 16);
}

// 33 /r - XOR r32, r/m32
void xor_r32_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    xor_r_rm(ctx, modrm, mem_addr, 32);
}

// REX.W + 33 /r - XOR r64, r/m64
void xor_r64_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;
    xor_r_rm(ctx, modrm, mem_addr, 64);
}

// --- XOR decode helpers ---

void decode_modrm_xor(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

// --- XOR instruction decoder ---

DecodedInstruction decode_xor_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    inst.has_lock_prefix = has_lock_prefix;

    switch (inst.opcode) {
    // 34 ib - XOR AL, imm8
    case 0x34:
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

    // 0D iw/id - XOR AX, imm16 / XOR EAX, imm32 / REX.W + XOR RAX, imm32
    case 0x35:
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
        for (int i = 0; i < inst.imm_size; i++) {
            inst.immediate |= ((uint64_t)code[offset++]) << (i * 8);
        }
        break;

    // 80 /6 ib - XOR r/m8, imm8
    case 0x80:
        inst.operand_size = 8;
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 6) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // 81 /6 iw/id - XOR r/m16, imm16 / XOR r/m32, imm32 / REX.W + XOR r/m64, imm32
    case 0x81:
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 6) {
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
        for (int i = 0; i < inst.imm_size; i++) {
            inst.immediate |= ((uint64_t)code[offset++]) << (i * 8);
        }
        break;

    // 83 /6 ib - XOR r/m16, imm8 / XOR r/m32, imm8 / REX.W + XOR r/m64, imm8
    case 0x83:
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 6) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // 30 /r - XOR r/m8, r8
    case 0x30:
        inst.operand_size = 8;
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 31 /r - XOR r/m16, r16 / XOR r/m32, r32 / REX.W + XOR r/m64, r64
    case 0x31:
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 32 /r - XOR r8, r/m8
    case 0x32:
        inst.operand_size = 8;
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    // 33 /r - XOR r16, r/m16 / XOR r32, r/m32 / REX.W + XOR r64, r/m64
    case 0x33:
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_xor(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

// --- XOR instruction executor ---

inline void execute_xor_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    // 34 ib - XOR AL, imm8
    case 0x34:
        xor_al_imm8(ctx, (uint8_t)inst.immediate);
        break;

    // 0D iw/id - XOR AX, imm16 / XOR EAX, imm32 / REX.W + XOR RAX, imm32
    case 0x35:
        if (ctx->rex_w) {
            xor_rax_imm32(ctx, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            xor_ax_imm16(ctx, (uint16_t)inst.immediate);
        }
        else {
            xor_eax_imm32(ctx, (uint32_t)inst.immediate);
        }
        break;

    // 80 /6 ib - XOR r/m8, imm8
    case 0x80:
        xor_rm8_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint8_t)inst.immediate, inst.has_lock_prefix);
        break;

    // 81 /6 iw/id - XOR r/m16, imm16 / XOR r/m32, imm32 / REX.W + XOR r/m64, imm32
    case 0x81:
        if (ctx->rex_w) {
            xor_rm64_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int32_t)inst.immediate, inst.has_lock_prefix);
        }
        else if (ctx->operand_size_override) {
            xor_rm16_imm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint16_t)inst.immediate, inst.has_lock_prefix);
        }
        else {
            xor_rm32_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint32_t)inst.immediate, inst.has_lock_prefix);
        }
        break;

    // 83 /6 ib - XOR r/m16, imm8 / XOR r/m32, imm8 / REX.W + XOR r/m64, imm8
    case 0x83:
        if (ctx->rex_w) {
            xor_rm64_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate, inst.has_lock_prefix);
        }
        else if (ctx->operand_size_override) {
            xor_rm16_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate, inst.has_lock_prefix);
        }
        else {
            xor_rm32_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate, inst.has_lock_prefix);
        }
        break;

    // 30 /r - XOR r/m8, r8
    case 0x30:
        xor_rm8_r8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.has_lock_prefix);
        break;

    // 31 /r - XOR r/m16, r16 / XOR r/m32, r32 / REX.W + XOR r/m64, r64
    case 0x31:
        if (ctx->rex_w) {
            xor_rm64_r64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.has_lock_prefix);
        }
        else if (ctx->operand_size_override) {
            xor_rm16_r16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.has_lock_prefix);
        }
        else {
            xor_rm32_r32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.has_lock_prefix);
        }
        break;

    // 32 /r - XOR r8, r/m8
    case 0x32:
        xor_r8_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    // 33 /r - XOR r16, r/m16 / XOR r32, r/m32 / REX.W + XOR r64, r/m64
    case 0x33:
        if (ctx->rex_w) {
            xor_r64_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            xor_r16_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            xor_r32_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    }
}

void execute_xor(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_xor_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_xor_with_decoded(ctx, &inst);
}

inline void execute_xor_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_xor_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_xor_with_decoded(ctx, &live);
}
