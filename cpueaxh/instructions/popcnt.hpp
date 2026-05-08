// instrusments/popcnt.hpp - POPCNT instruction implementation

static inline bool cpu_has_popcnt_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 1, 0);
    return (cpu_info[2] & (1 << 23)) != 0;
}

static inline int decode_popcnt_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static inline int decode_popcnt_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

static inline uint64_t read_popcnt_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    const uint8_t mod = (modrm >> 6) & 0x03;
    const int rm = decode_popcnt_rm_index(ctx, modrm);

    if (mod == 3) {
        switch (operand_size) {
        case 16: return get_reg16(ctx, rm);
        case 32: return get_reg32(ctx, rm);
        case 64: return get_reg64(ctx, rm);
        default: raise_ud_ctx(ctx); return 0;
        }
    }

    switch (operand_size) {
    case 16: return read_memory_word(ctx, mem_addr);
    case 32: return read_memory_dword(ctx, mem_addr);
    case 64: return read_memory_qword(ctx, mem_addr);
    default: raise_ud_ctx(ctx); return 0;
    }
}

static inline void write_popcnt_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size, uint64_t value) {
    const int reg = decode_popcnt_reg_index(ctx, modrm);
    switch (operand_size) {
    case 16:
        set_reg16(ctx, reg, (uint16_t)value);
        break;
    case 32:
        set_reg32(ctx, reg, (uint32_t)value);
        break;
    case 64:
        set_reg64(ctx, reg, value);
        break;
    default:
        raise_ud_ctx(ctx);
    }
}

static inline void decode_modrm_popcnt(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
    if (*offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    const uint8_t mod = (inst->modrm >> 6) & 0x03;
    const uint8_t rm = inst->modrm & 0x07;

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

static inline DecodedInstruction decode_popcnt_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    bool has_f3_prefix = false;
    bool has_unsupported_prefix = false;

    while (offset < code_size) {
        const uint8_t prefix = code[offset];
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
        else if (prefix == 0xF3) {
            has_f3_prefix = true;
            offset++;
        }
        else if (prefix == 0xF2) {
            has_unsupported_prefix = true;
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

    if (has_unsupported_prefix || !has_f3_prefix) {
        raise_ud_ctx(ctx);
    }

    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0xB8) {
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

    decode_modrm_popcnt(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline uint64_t popcnt_count_bits(uint64_t value, int operand_size) {
    uint64_t count = 0;
    for (int bit = 0; bit < operand_size; ++bit) {
        count += (value >> bit) & 0x1ULL;
    }
    return count;
}

static inline void execute_popcnt_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    if (!cpu_has_popcnt_feature()) {
        raise_ud_ctx(ctx);
        return;
    }

    const uint64_t source = read_popcnt_rm_operand(ctx, inst.modrm, inst.mem_address, inst.operand_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const uint64_t result = popcnt_count_bits(source, inst.operand_size);
    write_popcnt_reg_operand(ctx, inst.modrm, inst.operand_size, result);

    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_PF, false);
    set_flag(ctx, RFLAGS_AF, false);
    set_flag(ctx, RFLAGS_ZF, source == 0);
    set_flag(ctx, RFLAGS_SF, false);
    set_flag(ctx, RFLAGS_OF, false);
}

static inline void execute_popcnt(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    const DecodedInstruction inst = decode_popcnt_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_popcnt_with_decoded(ctx, &inst);
}

static inline void execute_popcnt_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_popcnt_with_decoded(ctx, &dec->cached);
}
