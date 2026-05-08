// instrusments/sse2_misc.hpp - MASKMOVDQU/LDDQU implementation

int decode_sse2_misc_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

int decode_sse2_misc_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

void decode_modrm_sse2_misc(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    if (inst->opcode == 0xF0 && mod == 3) {
        raise_ud_ctx(ctx);
    }

    if (inst->opcode == 0xF7 && mod != 3) {
        raise_ud_ctx(ctx);
    }

    if (mod != 3) {
        inst->mem_address = get_effective_address(ctx, inst->modrm, &inst->sib, &inst->displacement, inst->address_size);
    }
}

DecodedInstruction decode_sse2_misc_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
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
    bool has_unsupported_simd_prefix = false;
    *mandatory_prefix = 0;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            if (*mandatory_prefix == 0 || *mandatory_prefix == prefix) {
                *mandatory_prefix = prefix;
            }
            else {
                has_unsupported_simd_prefix = true;
            }
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
                 prefix == 0x64 || prefix == 0x65) {
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
    if (inst.opcode != 0xF0 && inst.opcode != 0xF7) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if ((inst.opcode == 0xF0 && *mandatory_prefix != 0xF2) ||
        (inst.opcode == 0xF7 && *mandatory_prefix != 0x66)) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse2_misc(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void sse2_misc_xmm_to_bytes(XMMRegister value, uint8_t bytes[16]) {
    for (int i = 0; i < 8; i++) {
        bytes[i] = (uint8_t)((value.low >> (i * 8)) & 0xFFU);
        bytes[8 + i] = (uint8_t)((value.high >> (i * 8)) & 0xFFU);
    }
}

void sse2_misc_write_maskmovdqu_bytes(CPU_CONTEXT* ctx, uint64_t address, const uint8_t src_bytes[16], const uint8_t mask_bytes[16]) {
    bool element_mask[16] = {};
    uint64_t element_values[16] = {};
    uint8_t* byte_ptrs[16] = {};

    for (int i = 0; i < 16; ++i) {
        element_mask[i] = (mask_bytes[i] & 0x80U) != 0;
        element_values[i] = src_bytes[i];
    }

    if (!cpu_resolve_masked_write_element_ptrs(ctx, address, element_mask, element_values, 16, 1, byte_ptrs)) {
        return;
    }

    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        for (int i = 0; i < 16; ++i) {
            if (element_mask[i]) {
                cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + (uint64_t)i, 1, src_bytes[i]);
            }
        }
    }

    cpu_commit_masked_write_elements(src_bytes, element_mask, 16, 1, byte_ptrs);
}

uint64_t get_sse2_misc_maskmovdqu_address(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    switch (inst->address_size) {
    case 16:
        return (uint64_t)get_reg16(ctx, REG_RDI);
    case 32:
        return (uint64_t)get_reg32(ctx, REG_RDI);
    default:
        return get_reg64(ctx, REG_RDI);
    }
}

void execute_lddqu_misc(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    int dest = decode_sse2_misc_xmm_reg_index(ctx, inst->modrm);
    set_xmm128(ctx, dest, read_xmm_memory(ctx, inst->mem_address));
}

void execute_maskmovdqu_misc(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    int src = decode_sse2_misc_xmm_reg_index(ctx, inst->modrm);
    int mask = decode_sse2_misc_xmm_rm_index(ctx, inst->modrm);
    uint8_t src_bytes[16] = {};
    uint8_t mask_bytes[16] = {};
    uint64_t address = get_sse2_misc_maskmovdqu_address(ctx, inst);

    sse2_misc_xmm_to_bytes(get_xmm128(ctx, src), src_bytes);
    sse2_misc_xmm_to_bytes(get_xmm128(ctx, mask), mask_bytes);

    sse2_misc_write_maskmovdqu_bytes(ctx, address, src_bytes, mask_bytes);
}

void execute_sse2_misc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    DecodedInstruction inst = decode_sse2_misc_instruction(ctx, code, code_size, &mandatory_prefix);

    switch (inst.opcode) {
    case 0xF0:
        execute_lddqu_misc(ctx, &inst);
        return;
    case 0xF7:
        execute_maskmovdqu_misc(ctx, &inst);
        return;
    default:
        raise_ud_ctx(ctx);
    }
}
