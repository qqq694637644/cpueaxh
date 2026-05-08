// instrusments/rdssp.hpp - RDSSPD/RDSSPQ instruction implementation

inline bool probe_rdssp_instruction(const uint8_t* code, size_t code_size, bool* is_64bit = NULL, uint32_t* instruction_size = NULL) {
    if (!code) {
        return false;
    }

    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_f3_prefix = false;
    bool rex_w = false;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0x67 ||
            prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65 || prefix == 0xF2) {
            offset++;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            rex_w = ((prefix >> 3) & 1) != 0;
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
        else {
            break;
        }
    }

    if (offset + 3 > code_size) {
        return false;
    }

    uint8_t modrm = code[offset + 2];
    uint8_t mod = (uint8_t)(modrm >> 6);
    uint8_t reg = (uint8_t)((modrm >> 3) & 0x07);

    if (has_lock_prefix || !has_f3_prefix ||
        code[offset] != 0x0F || code[offset + 1] != 0x1E ||
        reg != 1 || mod != 3) {
        return false;
    }

    if (is_64bit) {
        *is_64bit = rex_w;
    }
    if (instruction_size) {
        *instruction_size = (uint32_t)(offset + 3);
    }
    return true;
}

inline bool decode_rdssp_common(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, DecodedInstruction* out_inst) {
    if (!ctx || !code || !out_inst) {
        return false;
    }

    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_f3_prefix = false;

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
            ctx->rex_w = ((prefix >> 3) & 1) != 0;
            ctx->rex_r = ((prefix >> 2) & 1) != 0;
            ctx->rex_x = ((prefix >> 1) & 1) != 0;
            ctx->rex_b = (prefix & 1) != 0;
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
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2) {
            offset++;
        }
        else {
            break;
        }
    }

    if (offset + 3 > code_size) {
        raise_gp_ctx(ctx, 0);
        return false;
    }

    if (code[offset++] != 0x0F || code[offset++] != 0x1E) {
        raise_ud_ctx(ctx);
        return false;
    }

    inst.has_modrm = true;
    inst.modrm = code[offset++];
    uint8_t mod = (uint8_t)(inst.modrm >> 6);
    uint8_t reg = (uint8_t)((inst.modrm >> 3) & 0x07);

    if (has_lock_prefix || !has_f3_prefix || reg != 1 || mod != 3) {
        raise_ud_ctx(ctx);
        return false;
    }

    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    *out_inst = inst;
    return !cpu_has_exception(ctx);
}

inline bool is_rdssp_instruction(const uint8_t* code, size_t code_size, bool* is_64bit = NULL, uint32_t* instruction_size = NULL) {
    return probe_rdssp_instruction(code, code_size, is_64bit, instruction_size);
}

inline DecodedInstruction decode_rdssp_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    if (!decode_rdssp_common(ctx, code, code_size, &inst)) {
        return inst;
    }

    finalize_rip_relative_address(ctx, &inst, inst.inst_size);
    ctx->last_inst_size = inst.inst_size;
    return inst;
}

inline void execute_rdssp(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    if (!ctx) {
        return;
    }

    uint32_t instruction_size = 0;
    if (!probe_rdssp_instruction(code, code_size, NULL, &instruction_size)) {
        raise_ud_ctx(ctx);
        return;
    }

    ctx->last_inst_size = (int)instruction_size;
}
