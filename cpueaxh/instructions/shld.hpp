// instrusments/shld.hpp - SHLD/SHRD instruction implementation

int decode_shld_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

uint64_t read_shld_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size) {
    int reg = decode_shld_reg_index(ctx, modrm);
    switch (operand_size) {
    case 16: return get_reg16(ctx, reg);
    case 32: return get_reg32(ctx, reg);
    case 64: return get_reg64(ctx, reg);
    default: raise_ud_ctx(ctx); return 0;
    }
}

uint8_t decode_shld_count(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    switch (inst->opcode) {
    case 0xA4:
    case 0xAC:
        return (uint8_t)inst->immediate;
    case 0xA5:
    case 0xAD:
        return get_reg8(ctx, REG_RCX);
    default:
        raise_ud_ctx(ctx);
        return 0;
    }
}

void update_flags_shld_shrd(CPU_CONTEXT* ctx, uint64_t old_value, uint64_t result, int operand_size, unsigned int count, bool is_shrd) {
    if (count == 0) {
        return;
    }

    uint64_t sign_mask = get_shl_sign_mask(ctx, operand_size);
    uint64_t carry_bit = is_shrd
        ? ((old_value >> (count - 1)) & 0x01ULL)
        : ((old_value >> (operand_size - count)) & 0x01ULL);
    set_flag(ctx, RFLAGS_CF, carry_bit != 0);
    if (count == 1) {
        bool old_msb = (old_value & sign_mask) != 0;
        bool new_msb = (result & sign_mask) != 0;
        set_flag(ctx, RFLAGS_OF, old_msb ^ new_msb);
    }
    set_flag(ctx, RFLAGS_SF, (result & sign_mask) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

void shld_shrd_rm(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int operand_size, uint8_t raw_count, bool is_shrd) {
    (void)sib;
    (void)disp;

    uint64_t mask = get_shl_operand_mask(ctx, operand_size);
    unsigned int count = raw_count & get_shl_count_mask(operand_size);
    uint64_t old_value = read_shl_rm_operand(ctx, modrm, mem_addr, operand_size) & mask;
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (count == 0) {
        return;
    }

    uint64_t source = read_shld_reg_operand(ctx, modrm, operand_size) & mask;
    if (cpu_has_exception(ctx)) {
        return;
    }

    if (count > (unsigned int)operand_size) {
        count %= (unsigned int)operand_size;
        if (count == 0) {
            count = (unsigned int)operand_size;
        }
    }

    uint64_t result = 0;
    if (is_shrd) {
        result = (count == (unsigned int)operand_size)
            ? source
            : ((old_value >> count) | (source << (operand_size - count)));
    }
    else {
        result = (count == (unsigned int)operand_size)
            ? source
            : ((old_value << count) | (source >> (operand_size - count)));
    }
    result &= mask;

    write_shl_rm_operand(ctx, modrm, mem_addr, operand_size, result);
    if (cpu_has_exception(ctx)) {
        return;
    }
    update_flags_shld_shrd(ctx, old_value, result, operand_size, count, is_shrd);
}

DecodedInstruction decode_shld_shrd_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (offset + 1 >= code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
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
    case 0xA4:
    case 0xA5:
    case 0xAC:
    case 0xAD:
        decode_modrm_shl(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (has_lock_prefix) {
            raise_ud_ctx(ctx);
        }
        if (inst.opcode == 0xA4 || inst.opcode == 0xAC) {
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
        break;
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline DecodedInstruction decode_shld_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    return decode_shld_shrd_instruction(ctx, code, code_size);
}

inline DecodedInstruction decode_shrd_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    return decode_shld_shrd_instruction(ctx, code, code_size);
}

inline void execute_shld_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    DecodedInstruction inst = *inst_ptr;
    uint8_t count = decode_shld_count(ctx, &inst);
    shld_shrd_rm(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.operand_size, count, false);
}

inline void execute_shrd_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    DecodedInstruction inst = *inst_ptr;
    uint8_t count = decode_shld_count(ctx, &inst);
    shld_shrd_rm(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.operand_size, count, true);
}

void execute_shld(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_shld_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_shld_with_decoded(ctx, &inst);
}

void execute_shrd(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_shrd_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_shrd_with_decoded(ctx, &inst);
}

inline void execute_shld_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_shld_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_shld_with_decoded(ctx, &live);
}

inline void execute_shrd_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_shrd_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_shrd_with_decoded(ctx, &live);
}
