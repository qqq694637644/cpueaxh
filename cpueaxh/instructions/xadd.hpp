// instrusments/xadd.hpp - XADD instruction implementation

int decode_xadd_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg_index = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg_index |= 0x08;
    }
    return reg_index;
}

int decode_xadd_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm_index = modrm & 0x07;
    if (ctx->rex_b) {
        rm_index |= 0x08;
    }
    return rm_index;
}

uint64_t read_xadd_reg_operand(CPU_CONTEXT* ctx, int reg_index, int operand_size) {
    switch (operand_size) {
    case 8:  return get_reg8(ctx, reg_index);
    case 16: return get_reg16(ctx, reg_index);
    case 32: return get_reg32(ctx, reg_index);
    case 64: return get_reg64(ctx, reg_index);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_xadd_reg_operand(CPU_CONTEXT* ctx, int reg_index, int operand_size, uint64_t value) {
    switch (operand_size) {
    case 8:  set_reg8(ctx, reg_index, (uint8_t)value); break;
    case 16: set_reg16(ctx, reg_index, (uint16_t)value); break;
    case 32: set_reg32(ctx, reg_index, (uint32_t)value); break;
    case 64: set_reg64(ctx, reg_index, value); break;
    default: raise_ud_ctx(ctx);
    }
}

uint64_t read_xadd_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm_index = decode_xadd_rm_index(ctx, modrm);
    if (mod == 3) {
        return read_xadd_reg_operand(ctx, rm_index, operand_size);
    }

    switch (operand_size) {
    case 8:  return read_memory_byte(ctx, mem_addr);
    case 16: return read_memory_word(ctx, mem_addr);
    case 32: return read_memory_dword(ctx, mem_addr);
    case 64: return read_memory_qword(ctx, mem_addr);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_xadd_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size, uint64_t value) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm_index = decode_xadd_rm_index(ctx, modrm);
    if (mod == 3) {
        write_xadd_reg_operand(ctx, rm_index, operand_size, value);
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

uint64_t xadd_compute_result(CPU_CONTEXT* ctx, int operand_size, uint64_t dst_value, uint64_t src_value) {
    switch (operand_size) {
    case 8: {
        uint8_t dst = (uint8_t)dst_value;
        uint8_t src = (uint8_t)src_value;
        uint16_t result = (uint16_t)dst + (uint16_t)src;
        update_flags_add8(ctx, dst, src, result);
        return (uint8_t)result;
    }
    case 16: {
        uint16_t dst = (uint16_t)dst_value;
        uint16_t src = (uint16_t)src_value;
        uint32_t result = (uint32_t)dst + (uint32_t)src;
        update_flags_add16(ctx, dst, src, result);
        return (uint16_t)result;
    }
    case 32: {
        uint32_t dst = (uint32_t)dst_value;
        uint32_t src = (uint32_t)src_value;
        uint64_t result = (uint64_t)dst + (uint64_t)src;
        update_flags_add32(ctx, dst, src, result);
        return (uint32_t)result;
    }
    case 64: {
        uint64_t result = dst_value + src_value;
        update_flags_add64(ctx, dst_value, src_value, result);
        return result;
    }
    default:
        raise_ud_ctx(ctx);
        return 0;
    }
}

void xadd_rm_reg(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size, bool has_lock_prefix) {
    int reg_index = decode_xadd_reg_index(ctx, modrm);
    uint64_t src_value = read_xadd_reg_operand(ctx, reg_index, operand_size);

    if (has_lock_prefix && ((modrm >> 6) & 0x03) != 3) {
        uint64_t dst_value = 0;
        uint64_t result = 0;
        if (!cpu_atomic_add_memory(ctx, mem_addr, operand_size, src_value, &dst_value, &result)) {
            return;
        }
        xadd_compute_result(ctx, operand_size, dst_value, src_value);
        write_xadd_reg_operand(ctx, reg_index, operand_size, dst_value);
        return;
    }

    uint64_t dst_value = read_xadd_rm_operand(ctx, modrm, mem_addr, operand_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    uint64_t result = xadd_compute_result(ctx, operand_size, dst_value, src_value);

    write_xadd_reg_operand(ctx, reg_index, operand_size, dst_value);
    write_xadd_rm_operand(ctx, modrm, mem_addr, operand_size, result);
}

void decode_modrm_xadd(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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
        for (int index = 0; index < inst->disp_size; index++) {
            inst->displacement |= ((int32_t)code[(*offset)++]) << (index * 8);
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

DecodedInstruction decode_xadd_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

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

    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
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
    case 0xC0:
        inst.operand_size = 8;
        decode_modrm_xadd(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;
    case 0xC1:
        decode_modrm_xadd(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        break;
    default:
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_xadd_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    xadd_rm_reg(ctx, inst.modrm, inst.mem_address, inst.operand_size, inst.has_lock_prefix);
}

void execute_xadd(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_xadd_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_xadd_with_decoded(ctx, &inst);
}

inline void execute_xadd_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    if (!decoded_inst_needs_mem_recompute(&dec->cached)) {
        execute_xadd_with_decoded(ctx, &dec->cached);
        return;
    }
    DecodedInstruction live = dec->cached;
    live.mem_address = get_effective_address(ctx, live.modrm, &live.sib, &live.displacement,
                                             live.address_size, dec->length);
    execute_xadd_with_decoded(ctx, &live);
}
