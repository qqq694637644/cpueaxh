#pragma once

// instrusments/bsf.hpp - BSF/BSR instruction implementation

int decode_bsf_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

int decode_bsf_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

uint64_t read_bsf_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_bsf_rm_index(ctx, modrm);

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

void write_bsf_reg_operand(CPU_CONTEXT* ctx, uint8_t modrm, int operand_size, uint64_t value) {
    int reg = decode_bsf_reg_index(ctx, modrm);

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

uint64_t get_bsf_operand_mask(CPU_CONTEXT* ctx, int operand_size) {
    switch (operand_size) {
    case 16: return 0xFFFFULL;
    case 32: return 0xFFFFFFFFULL;
    case 64: return 0xFFFFFFFFFFFFFFFFULL;
    default: raise_ud_ctx(ctx); return 0;
    }
}

static inline bool cpu_has_bmi1_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 3)) != 0;
}

static inline bool cpu_has_lzcnt_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0x80000000u, 0);
    if (static_cast<uint32_t>(cpu_info[0]) < 0x80000001u) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 0x80000001u, 0);
    return (cpu_info[2] & (1 << 5)) != 0;
}

uint64_t scan_forward_bsf(uint64_t value, int operand_size) {
    for (int bit = 0; bit < operand_size; bit++) {
        if ((value >> bit) & 0x1ULL) {
            return (uint64_t)bit;
        }
    }

    return 0;
}

uint64_t scan_reverse_bsf(uint64_t value, int operand_size) {
    for (int bit = operand_size - 1; bit >= 0; bit--) {
        if ((value >> bit) & 0x1ULL) {
            return (uint64_t)bit;
        }
    }

    return 0;
}

uint64_t count_trailing_zeros_bsf(uint64_t value, int operand_size) {
    for (int bit = 0; bit < operand_size; bit++) {
        if (((value >> bit) & 0x1ULL) != 0) {
            return (uint64_t)bit;
        }
    }

    return (uint64_t)operand_size;
}

uint64_t count_leading_zeros_bsf(uint64_t value, int operand_size) {
    for (int bit = operand_size - 1; bit >= 0; bit--) {
        if (((value >> bit) & 0x1ULL) != 0) {
            return (uint64_t)(operand_size - 1 - bit);
        }
    }

    return (uint64_t)operand_size;
}

void decode_modrm_bsf(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

DecodedInstruction decode_bsf_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
        else if (prefix == 0xF3) {
            inst.mandatory_prefix = prefix;
            offset++;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2) {
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
    if (inst.opcode != 0xBC && inst.opcode != 0xBD) {
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

    decode_modrm_bsf(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_bsf_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    uint64_t source = read_bsf_rm_operand(ctx, inst.modrm, inst.mem_address, inst.operand_size) & get_bsf_operand_mask(ctx, inst.operand_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const bool f3_is_tzcnt_lzcnt = inst.mandatory_prefix == 0xF3 &&
        ((inst.opcode == 0xBC && cpu_has_bmi1_feature()) ||
         (inst.opcode == 0xBD && cpu_has_lzcnt_feature()));

    if (f3_is_tzcnt_lzcnt) {
        uint64_t result = (inst.opcode == 0xBC)
            ? count_trailing_zeros_bsf(source, inst.operand_size)
            : count_leading_zeros_bsf(source, inst.operand_size);

        write_bsf_reg_operand(ctx, inst.modrm, inst.operand_size, result);
        set_flag(ctx, RFLAGS_CF, source == 0);
        set_flag(ctx, RFLAGS_ZF, result == 0);
        return;
    }

    if (source == 0) {
        set_flag(ctx, RFLAGS_ZF, true);
        return;
    }

    uint64_t result = (inst.opcode == 0xBC)
        ? scan_forward_bsf(source, inst.operand_size)
        : scan_reverse_bsf(source, inst.operand_size);

    write_bsf_reg_operand(ctx, inst.modrm, inst.operand_size, result);
    set_flag(ctx, RFLAGS_ZF, false);
}

void execute_bsf(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_bsf_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_bsf_with_decoded(ctx, &inst);
}

inline void execute_bsf_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_bsf_with_decoded(ctx, &dec->cached);
}
