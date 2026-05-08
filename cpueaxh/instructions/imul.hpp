// instrusments/imul.hpp - IMUL instruction implementation

// --- IMUL internal helpers ---

int decode_imul_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_imul_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

uint64_t read_imul_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_imul_rm_index(ctx, modrm);

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

uint64_t read_imul_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size) {
    int reg = decode_imul_reg_index(ctx, modrm);

    switch (operand_size) {
    case 16: return get_reg16(ctx, reg);
    case 32: return get_reg32(ctx, reg);
    case 64: return get_reg64(ctx, reg);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_imul_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size, uint64_t value) {
    int reg = decode_imul_reg_index(ctx, modrm);

    switch (operand_size) {
    case 16: set_reg16(ctx, reg, (uint16_t)value); break;
    case 32: set_reg32(ctx, reg, (uint32_t)value); break;
    case 64: set_reg64(ctx, reg, value); break;
    default: raise_ud_ctx(ctx);
    }
}

void update_flags_imul(CPU_CONTEXT* ctx, bool overflow) {
    set_flag(ctx, RFLAGS_CF, overflow);
    set_flag(ctx, RFLAGS_OF, overflow);
}

// --- IMUL execution helpers ---

// F6 /5 - IMUL r/m8
void imul_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int8_t lhs = (int8_t)get_reg8(ctx, REG_RAX);
    int8_t rhs = (int8_t)read_imul_rm_operand(ctx, modrm, mem_addr, 8);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int16_t result = (int16_t)lhs * (int16_t)rhs;

    set_reg16(ctx, REG_RAX, (uint16_t)result);
    update_flags_imul(ctx, result != (int16_t)(int8_t)result);
}

// F7 /5 - IMUL r/m16
void imul_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int16_t lhs = (int16_t)get_reg16(ctx, REG_RAX);
    int16_t rhs = (int16_t)read_imul_rm_operand(ctx, modrm, mem_addr, 16);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int32_t result = (int32_t)lhs * (int32_t)rhs;

    set_reg16(ctx, REG_RAX, (uint16_t)result);
    set_reg16(ctx, REG_RDX, (uint16_t)((uint32_t)result >> 16));
    update_flags_imul(ctx, result != (int32_t)(int16_t)result);
}

// F7 /5 - IMUL r/m32
void imul_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int32_t lhs = (int32_t)get_reg32(ctx, REG_RAX);
    int32_t rhs = (int32_t)read_imul_rm_operand(ctx, modrm, mem_addr, 32);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int64_t result = (int64_t)lhs * (int64_t)rhs;

    set_reg32(ctx, REG_RAX, (uint32_t)result);
    set_reg32(ctx, REG_RDX, (uint32_t)((uint64_t)result >> 32));
    update_flags_imul(ctx, result != (int64_t)(int32_t)result);
}

// REX.W + F7 /5 - IMUL r/m64
void imul_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int64_t lhs = (int64_t)get_reg64(ctx, REG_RAX);
    int64_t rhs = (int64_t)read_imul_rm_operand(ctx, modrm, mem_addr, 64);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int64_t high = 0;
    int64_t low = _mul128(lhs, rhs, &high);

    set_reg64(ctx, REG_RAX, (uint64_t)low);
    set_reg64(ctx, REG_RDX, (uint64_t)high);
    update_flags_imul(ctx, high != (low < 0 ? -1LL : 0LL));
}

// 0F AF /r - IMUL r16, r/m16
void imul_r16_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int16_t lhs = (int16_t)read_imul_reg_operand(ctx, modrm, 16);
    int16_t rhs = (int16_t)read_imul_rm_operand(ctx, modrm, mem_addr, 16);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int32_t result = (int32_t)lhs * (int32_t)rhs;

    write_imul_reg_operand(ctx, modrm, 16, (uint16_t)result);
    update_flags_imul(ctx, result != (int32_t)(int16_t)result);
}

// 0F AF /r - IMUL r32, r/m32
void imul_r32_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int32_t lhs = (int32_t)read_imul_reg_operand(ctx, modrm, 32);
    int32_t rhs = (int32_t)read_imul_rm_operand(ctx, modrm, mem_addr, 32);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int64_t result = (int64_t)lhs * (int64_t)rhs;

    write_imul_reg_operand(ctx, modrm, 32, (uint32_t)result);
    update_flags_imul(ctx, result != (int64_t)(int32_t)result);
}

// REX.W + 0F AF /r - IMUL r64, r/m64
void imul_r64_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    int64_t lhs = (int64_t)read_imul_reg_operand(ctx, modrm, 64);
    int64_t rhs = (int64_t)read_imul_rm_operand(ctx, modrm, mem_addr, 64);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int64_t high = 0;
    int64_t low = _mul128(lhs, rhs, &high);

    write_imul_reg_operand(ctx, modrm, 64, (uint64_t)low);
    update_flags_imul(ctx, high != (low < 0 ? -1LL : 0LL));
}

// 6B /r ib - IMUL r16, r/m16, imm8
void imul_r16_rm16_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    (void)sib;
    (void)disp;

    int16_t src = (int16_t)read_imul_rm_operand(ctx, modrm, mem_addr, 16);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int16_t factor = (int16_t)imm;
    int32_t result = (int32_t)src * (int32_t)factor;

    write_imul_reg_operand(ctx, modrm, 16, (uint16_t)result);
    update_flags_imul(ctx, result != (int32_t)(int16_t)result);
}

// 6B /r ib - IMUL r32, r/m32, imm8
void imul_r32_rm32_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    (void)sib;
    (void)disp;

    int32_t src = (int32_t)read_imul_rm_operand(ctx, modrm, mem_addr, 32);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int32_t factor = (int32_t)imm;
    int64_t result = (int64_t)src * (int64_t)factor;

    write_imul_reg_operand(ctx, modrm, 32, (uint32_t)result);
    update_flags_imul(ctx, result != (int64_t)(int32_t)result);
}

// REX.W + 6B /r ib - IMUL r64, r/m64, imm8
void imul_r64_rm64_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int8_t imm) {
    (void)sib;
    (void)disp;

    int64_t src = (int64_t)read_imul_rm_operand(ctx, modrm, mem_addr, 64);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int64_t factor = (int64_t)imm;
    int64_t high = 0;
    int64_t low = _mul128(src, factor, &high);

    write_imul_reg_operand(ctx, modrm, 64, (uint64_t)low);
    update_flags_imul(ctx, high != (low < 0 ? -1LL : 0LL));
}

// 69 /r iw - IMUL r16, r/m16, imm16
void imul_r16_rm16_imm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint16_t imm) {
    (void)sib;
    (void)disp;

    int16_t src = (int16_t)read_imul_rm_operand(ctx, modrm, mem_addr, 16);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int16_t factor = (int16_t)imm;
    int32_t result = (int32_t)src * (int32_t)factor;

    write_imul_reg_operand(ctx, modrm, 16, (uint16_t)result);
    update_flags_imul(ctx, result != (int32_t)(int16_t)result);
}

// 69 /r id - IMUL r32, r/m32, imm32
void imul_r32_rm32_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint32_t imm) {
    (void)sib;
    (void)disp;

    int32_t src = (int32_t)read_imul_rm_operand(ctx, modrm, mem_addr, 32);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int32_t factor = (int32_t)imm;
    int64_t result = (int64_t)src * (int64_t)factor;

    write_imul_reg_operand(ctx, modrm, 32, (uint32_t)result);
    update_flags_imul(ctx, result != (int64_t)(int32_t)result);
}

// REX.W + 69 /r id - IMUL r64, r/m64, imm32
void imul_r64_rm64_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int32_t imm) {
    (void)sib;
    (void)disp;

    int64_t src = (int64_t)read_imul_rm_operand(ctx, modrm, mem_addr, 64);
    if (cpu_has_exception(ctx)) {
        return;
    }
    int64_t factor = (int64_t)imm;
    int64_t high = 0;
    int64_t low = _mul128(src, factor, &high);

    write_imul_reg_operand(ctx, modrm, 64, (uint64_t)low);
    update_flags_imul(ctx, high != (low < 0 ? -1LL : 0LL));
}

// --- IMUL decode helpers ---

void decode_modrm_imul(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

// --- IMUL instruction decoder ---

DecodedInstruction decode_imul_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (inst.opcode == 0x0F) {
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.opcode = code[offset++];
        if (inst.opcode != 0xAF) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_imul(ctx, &inst, code, code_size, &offset, has_lock_prefix);
    }
    else {
        switch (inst.opcode) {
        // F6 /5 - IMUL r/m8
        case 0xF6:
            inst.operand_size = 8;
            decode_modrm_imul(ctx, &inst, code, code_size, &offset, has_lock_prefix);
            if (((inst.modrm >> 3) & 0x07) != 5) {
                raise_ud_ctx(ctx);
            }
            break;

        // F7 /5 - IMUL r/m16 / r/m32 / r/m64
        case 0xF7:
            decode_modrm_imul(ctx, &inst, code, code_size, &offset, has_lock_prefix);
            if (((inst.modrm >> 3) & 0x07) != 5) {
                raise_ud_ctx(ctx);
            }
            break;

        // 6B /r ib - IMUL r16/32/64, r/m16/32/64, imm8
        case 0x6B:
            decode_modrm_imul(ctx, &inst, code, code_size, &offset, has_lock_prefix);
            inst.imm_size = 1;
            if (offset + inst.imm_size > code_size) {
                raise_gp_ctx(ctx, 0);
return inst;
            }
            inst.immediate = code[offset++];
            break;

        // 69 /r iw/id - IMUL r16/32/64, r/m16/32/64, imm16/32
        case 0x69:
            decode_modrm_imul(ctx, &inst, code, code_size, &offset, has_lock_prefix);
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

        default:
            raise_ud_ctx(ctx);
        }
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

// --- IMUL instruction executor ---

inline void execute_imul_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    case 0xF6:
        imul_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    case 0xF7:
        if (ctx->rex_w) {
            imul_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            imul_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            imul_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;

    case 0xAF:
        if (ctx->rex_w) {
            imul_r64_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            imul_r16_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            imul_r32_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;

    case 0x6B:
        if (ctx->rex_w) {
            imul_r64_rm64_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            imul_r16_rm16_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        else {
            imul_r32_rm32_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int8_t)inst.immediate);
        }
        break;

    case 0x69:
        if (ctx->rex_w) {
            imul_r64_rm64_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            imul_r16_rm16_imm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint16_t)inst.immediate);
        }
        else {
            imul_r32_rm32_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint32_t)inst.immediate);
        }
        break;
    }
}

void execute_imul(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_imul_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_imul_with_decoded(ctx, &inst);
}

inline void execute_imul_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_imul_with_decoded(ctx, &dec->cached);
}
