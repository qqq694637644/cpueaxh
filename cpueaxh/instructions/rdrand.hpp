// instrusments/rdrand.hpp - RDRAND instruction implementation

int decode_rdrand_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static inline bool cpu_has_rdrand_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 1, 0);
    return (cpu_info[2] & (1 << 30)) != 0;
}

bool is_rdrand_instruction(const uint8_t* code, size_t code_size) {
    size_t offset = 0;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0x67 || prefix == 0xF0 ||
            (prefix >= 0x40 && prefix <= 0x4F) ||
            prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
            offset++;
        }
        else {
            break;
        }
    }

    if (offset + 3 > code_size) {
        return false;
    }

    if (code[offset] != 0x0F || code[offset + 1] != 0xC7) {
        return false;
    }

    uint8_t modrm = code[offset + 2];
    return (((modrm >> 3) & 0x07) == 6) && (((modrm >> 6) & 0x03) == 3);
}

DecodedInstruction decode_rdrand_instruction(CPU_CONTEXT* ctx, const uint8_t* code, size_t code_size) {
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

    if (has_lock_prefix || !cpu_has_rdrand_feature()) {
        raise_ud_ctx(ctx);
    }

    if (offset + 3 > code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0xC7) {
        raise_ud_ctx(ctx);
    }

    inst.has_modrm = true;
    inst.modrm = code[offset++];
    if (((inst.modrm >> 3) & 0x07) != 6 || ((inst.modrm >> 6) & 0x03) != 3) {
        raise_ud_ctx(ctx);
    }

    if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }
    else {
        inst.operand_size = 32;
    }

    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_rdrand_register(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    int rm = decode_rdrand_rm_index(ctx, inst->modrm);

    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, false);
    set_flag(ctx, RFLAGS_ZF, false);
    set_flag(ctx, RFLAGS_AF, false);
    set_flag(ctx, RFLAGS_PF, false);

    if (inst->operand_size == 64) {
        unsigned __int64 value = 0;
        unsigned char ok = _rdrand64_step(&value);
        set_reg64(ctx, rm, ok ? (uint64_t)value : 0ULL);
        set_flag(ctx, RFLAGS_CF, ok != 0);
        return;
    }

    if (inst->operand_size == 32) {
        unsigned int value = 0;
        unsigned char ok = _rdrand32_step(&value);
        set_reg32(ctx, rm, ok ? (uint32_t)value : 0U);
        set_flag(ctx, RFLAGS_CF, ok != 0);
        return;
    }

    if (inst->operand_size == 16) {
        unsigned short value = 0;
        unsigned char ok = _rdrand16_step(&value);
        set_reg16(ctx, rm, ok ? (uint16_t)value : 0U);
        set_flag(ctx, RFLAGS_CF, ok != 0);
        return;
    }

    raise_ud_ctx(ctx);
}

void execute_rdrand(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_rdrand_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_rdrand_register(ctx, &inst);
}
