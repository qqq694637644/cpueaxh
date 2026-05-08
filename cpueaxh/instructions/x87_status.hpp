// instructions/x87_status.hpp - FINIT/FNINIT and FSTSW/FNSTSW implementation

static void decode_x87_status_modrm(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
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
        for (int index = 0; index < inst->disp_size; ++index) {
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
}

static size_t x87_status_parse_prefixes(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, bool* has_lock_prefix, bool* has_simd_prefix) {
    size_t offset = 0;
    *has_lock_prefix = false;
    *has_simd_prefix = false;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            *has_simd_prefix = true;
            ++offset;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            ++offset;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            ctx->rex_present = true;
            ctx->rex_w = (prefix >> 3) & 1;
            ctx->rex_r = (prefix >> 2) & 1;
            ctx->rex_x = (prefix >> 1) & 1;
            ctx->rex_b = prefix & 1;
            ++offset;
        }
        else if (prefix == 0xF0) {
            *has_lock_prefix = true;
            ++offset;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65) {
            ++offset;
        }
        else {
            break;
        }
    }

    return offset;
}

static DecodedInstruction decode_x87_init_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    bool has_lock_prefix = false;
    bool has_simd_prefix = false;
    size_t offset = x87_status_parse_prefixes(ctx, code, code_size, &has_lock_prefix, &has_simd_prefix);
    bool has_wait_prefix = false;

    if (offset < code_size && code[offset] == 0x9B) {
        has_wait_prefix = true;
        ++offset;
    }

    if (has_lock_prefix || has_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (offset + 1 >= code_size || code[offset] != 0xDB || code[offset + 1] != 0xE3) {
        raise_ud_ctx(ctx);
    }

    if (cpu_has_exception(ctx)) {
        return inst;
    }
    if (!cpu_x87_check_device_available(ctx) || !cpu_x87_check_wait(ctx, has_wait_prefix)) {
        return inst;
    }

    offset += 2;
    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

static DecodedInstruction decode_x87_status_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    bool has_lock_prefix = false;
    bool has_simd_prefix = false;
    size_t offset = x87_status_parse_prefixes(ctx, code, code_size, &has_lock_prefix, &has_simd_prefix);
    bool has_wait_prefix = false;

    if (offset < code_size && code[offset] == 0x9B) {
        has_wait_prefix = true;
        ++offset;
    }

    if (has_lock_prefix || has_simd_prefix || offset >= code_size) {
        raise_ud_ctx(ctx);
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0xDD && inst.opcode != 0xDF) {
        raise_ud_ctx(ctx);
    }

    if (cpu_has_exception(ctx)) {
        return inst;
    }
    if (!cpu_x87_check_device_available(ctx) || !cpu_x87_check_wait(ctx, has_wait_prefix)) {
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    if (inst.opcode == 0xDD) {
        decode_x87_status_modrm(ctx, &inst, code, code_size, &offset);

        const uint8_t mod = (inst.modrm >> 6) & 0x03;
        const uint8_t reg = (inst.modrm >> 3) & 0x07;
        if (reg != 7 || mod == 3) {
            raise_ud_ctx(ctx);
        }

        finalize_rip_relative_address(ctx, &inst, (int)offset);
    }
    else if (offset >= code_size || code[offset++] != 0xE0) {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_x87_init(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    decode_x87_init_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    cpu_reset_x87_state(ctx);
}

void execute_x87_status(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_x87_status_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    if (inst.opcode == 0xDD) {
        write_memory_word(ctx, inst.mem_address, ctx->x87_status_word);
        return;
    }

    set_reg16(ctx, REG_RAX, ctx->x87_status_word);
}
