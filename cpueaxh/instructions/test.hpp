// instrusments/test.hpp - TEST instruction implementation

// --- TEST execution helpers ---

// A8 ib - TEST AL, imm8
void test_al_imm8(CPU_CONTEXT* ctx, uint8_t imm) {
    uint8_t result = (uint8_t)(ctx->regs[REG_RAX] & 0xFF) & imm;
    update_flags_test8(ctx, result);
}

// A9 iw - TEST AX, imm16
void test_ax_imm16(CPU_CONTEXT* ctx, uint16_t imm) {
    uint16_t result = (uint16_t)(ctx->regs[REG_RAX] & 0xFFFF) & imm;
    update_flags_test16(ctx, result);
}

// A9 id - TEST EAX, imm32
void test_eax_imm32(CPU_CONTEXT* ctx, uint32_t imm) {
    uint32_t result = (uint32_t)(ctx->regs[REG_RAX] & 0xFFFFFFFF) & imm;
    update_flags_test32(ctx, result);
}

// REX.W + A9 id - TEST RAX, imm32 (sign-extended to 64 bits)
void test_rax_imm32(CPU_CONTEXT* ctx, int32_t imm) {
    uint64_t result = ctx->regs[REG_RAX] & (uint64_t)(int64_t)imm;
    update_flags_test64(ctx, result);
}

// F6 /0 ib - TEST r/m8, imm8
void test_rm8_imm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint8_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint8_t src1;
    if (mod == 3) {
        src1 = get_reg8(ctx, rm);
    }
    else {
        src1 = read_memory_byte(ctx, mem_addr);
    }

    update_flags_test8(ctx, src1 & imm);
}

// F7 /0 iw - TEST r/m16, imm16
void test_rm16_imm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint16_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint16_t src1;
    if (mod == 3) {
        src1 = get_reg16(ctx, rm);
    }
    else {
        src1 = read_memory_word(ctx, mem_addr);
    }

    update_flags_test16(ctx, src1 & imm);
}

// F7 /0 id - TEST r/m32, imm32
void test_rm32_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, uint32_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint32_t src1;
    if (mod == 3) {
        src1 = get_reg32(ctx, rm);
    }
    else {
        src1 = read_memory_dword(ctx, mem_addr);
    }

    update_flags_test32(ctx, src1 & imm);
}

// REX.W + F7 /0 id - TEST r/m64, imm32 (sign-extended to 64 bits)
void test_rm64_imm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int32_t imm) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint64_t src1;
    if (mod == 3) {
        src1 = get_reg64(ctx, rm);
    }
    else {
        src1 = read_memory_qword(ctx, mem_addr);
    }

    uint64_t src2 = (uint64_t)(int64_t)imm;
    update_flags_test64(ctx, src1 & src2);
}

// 84 /r - TEST r/m8, r8
void test_rm8_r8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint8_t src2 = get_reg8(ctx, reg);
    uint8_t src1;
    if (mod == 3) {
        src1 = get_reg8(ctx, rm);
    }
    else {
        src1 = read_memory_byte(ctx, mem_addr);
    }

    update_flags_test8(ctx, src1 & src2);
}

// 85 /r - TEST r/m16, r16
void test_rm16_r16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint16_t src2 = get_reg16(ctx, reg);
    uint16_t src1;
    if (mod == 3) {
        src1 = get_reg16(ctx, rm);
    }
    else {
        src1 = read_memory_word(ctx, mem_addr);
    }

    update_flags_test16(ctx, src1 & src2);
}

// 85 /r - TEST r/m32, r32
void test_rm32_r32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint32_t src2 = get_reg32(ctx, reg);
    uint32_t src1;
    if (mod == 3) {
        src1 = get_reg32(ctx, rm);
    }
    else {
        src1 = read_memory_dword(ctx, mem_addr);
    }

    update_flags_test32(ctx, src1 & src2);
}

// REX.W + 85 /r - TEST r/m64, r64
void test_rm64_r64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t reg = (modrm >> 3) & 0x07;
    uint8_t rm = modrm & 0x07;

    if (ctx->rex_r) {
        reg |= 0x08;
    }
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    uint64_t src2 = get_reg64(ctx, reg);
    uint64_t src1;
    if (mod == 3) {
        src1 = get_reg64(ctx, rm);
    }
    else {
        src1 = read_memory_qword(ctx, mem_addr);
    }

    update_flags_test64(ctx, src1 & src2);
}

// --- Helper: decode ModR/M for TEST ---

void decode_modrm_test(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
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
}

// --- TEST instruction decoder ---

DecodedInstruction decode_test_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

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
            // LOCK prefix is #UD for TEST
            raise_ud_ctx(ctx);
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
    // A8 ib - TEST AL, imm8
    case 0xA8:
        inst.operand_size = 8;
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // A9 iw/id - TEST AX, imm16 / TEST EAX, imm32 / REX.W + TEST RAX, imm32
    case 0xA9:
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

    // F6 /0 ib - TEST r/m8, imm8
    case 0xF6:
        inst.operand_size = 8;
        decode_modrm_test(ctx, &inst, code, code_size, &offset);
        if (((inst.modrm >> 3) & 0x07) != 0) {
            raise_ud_ctx(ctx);
        }
        inst.imm_size = 1;
        if (offset + inst.imm_size > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = code[offset++];
        break;

    // F7 /0 iw/id - TEST r/m16, imm16 / TEST r/m32, imm32 / REX.W + TEST r/m64, imm32
    case 0xF7:
        decode_modrm_test(ctx, &inst, code, code_size, &offset);
        if (((inst.modrm >> 3) & 0x07) != 0) {
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

    // 84 /r - TEST r/m8, r8
    case 0x84:
        inst.operand_size = 8;
        decode_modrm_test(ctx, &inst, code, code_size, &offset);
        break;

    // 85 /r - TEST r/m16, r16 / TEST r/m32, r32 / REX.W + TEST r/m64, r64
    case 0x85:
        decode_modrm_test(ctx, &inst, code, code_size, &offset);
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

// --- TEST instruction executor ---

inline void execute_test_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    // A8 ib - TEST AL, imm8
    case 0xA8:
        test_al_imm8(ctx, (uint8_t)inst.immediate);
        break;

    // A9 iw/id - TEST AX, imm16 / TEST EAX, imm32 / REX.W + TEST RAX, imm32
    case 0xA9:
        if (ctx->rex_w) {
            test_rax_imm32(ctx, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            test_ax_imm16(ctx, (uint16_t)inst.immediate);
        }
        else {
            test_eax_imm32(ctx, (uint32_t)inst.immediate);
        }
        break;

    // F6 /0 ib - TEST r/m8, imm8
    case 0xF6:
        test_rm8_imm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint8_t)inst.immediate);
        break;

    // F7 /0 iw/id - TEST r/m16, imm16 / TEST r/m32, imm32 / REX.W + TEST r/m64, imm32
    case 0xF7:
        if (ctx->rex_w) {
            test_rm64_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (int32_t)inst.immediate);
        }
        else if (ctx->operand_size_override) {
            test_rm16_imm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint16_t)inst.immediate);
        }
        else {
            test_rm32_imm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, (uint32_t)inst.immediate);
        }
        break;

    // 84 /r - TEST r/m8, r8
    case 0x84:
        test_rm8_r8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    // 85 /r - TEST r/m16, r16 / TEST r/m32, r32 / REX.W + TEST r/m64, r64
    case 0x85:
        if (ctx->rex_w) {
            test_rm64_r64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            test_rm16_r16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            test_rm32_r32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    }
}

void execute_test(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_test_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_test_with_decoded(ctx, &inst);
}

inline void execute_test_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_test_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_test_with_decoded(ctx, &live);
}
