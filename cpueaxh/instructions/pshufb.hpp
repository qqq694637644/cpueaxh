#pragma once

// instrusments/pshufb.hpp - PSHUFB implementation

static int decode_pshufb_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

static int decode_pshufb_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static void decode_modrm_pshufb(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
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
}

static void validate_pshufb_xmm_alignment(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (!inst || ((inst->modrm >> 6) & 0x03) == 0x03) {
        return;
    }

    if ((inst->mem_address & 0x0FULL) != 0) {
        raise_gp_ctx(ctx, 0);
return;
    }
}

static void pshufb_qword_to_bytes(uint64_t value, uint8_t bytes[8]) {
    for (int index = 0; index < 8; index++) {
        bytes[index] = (uint8_t)((value >> (index * 8)) & 0xFFU);
    }
}

static uint64_t pshufb_bytes_to_qword(const uint8_t bytes[8]) {
    uint64_t value = 0;
    for (int index = 0; index < 8; index++) {
        value |= ((uint64_t)bytes[index]) << (index * 8);
    }
    return value;
}

static XMMRegister apply_pshufb128(XMMRegister source, XMMRegister control) {
    uint8_t source_bytes[16] = {};
    uint8_t control_bytes[16] = {};
    uint8_t result_bytes[16] = {};
    sse2_pack_xmm_to_bytes(source, source_bytes);
    sse2_pack_xmm_to_bytes(control, control_bytes);
    for (int index = 0; index < 16; index++) {
        uint8_t selector = control_bytes[index];
        result_bytes[index] = (selector & 0x80U) ? 0x00U : source_bytes[selector & 0x0FU];
    }
    return sse2_pack_bytes_to_xmm(result_bytes);
}

static uint64_t apply_pshufb64(uint64_t source, uint64_t control) {
    uint8_t source_bytes[8] = {};
    uint8_t control_bytes[8] = {};
    uint8_t result_bytes[8] = {};
    pshufb_qword_to_bytes(source, source_bytes);
    pshufb_qword_to_bytes(control, control_bytes);
    for (int index = 0; index < 8; index++) {
        uint8_t selector = control_bytes[index];
        result_bytes[index] = (selector & 0x80U) ? 0x00U : source_bytes[selector & 0x07U];
    }
    return pshufb_bytes_to_qword(result_bytes);
}

inline bool is_pshufb_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x38 && code[prefix_len + 2] == 0x00;
}

inline DecodedInstruction decode_pshufb_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, bool* xmm_form) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_unsupported_simd_prefix = false;
    uint8_t mandatory_prefix = 0;

    if (xmm_form) {
        *xmm_form = false;
    }

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
            if (mandatory_prefix == 0 || mandatory_prefix == prefix) {
                mandatory_prefix = prefix;
            }
            else {
                has_unsupported_simd_prefix = true;
            }
            if (prefix == 0x66) {
                ctx->operand_size_override = true;
            }
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
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E || prefix == 0x64 || prefix == 0x65) {
            offset++;
        }
        else {
            break;
        }
    }

    if (has_lock_prefix || has_unsupported_simd_prefix) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (mandatory_prefix != 0 && mandatory_prefix != 0x66) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (offset + 4 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset++] != 0x0F || code[offset++] != 0x38) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.opcode = code[offset++];
    inst.mandatory_prefix = mandatory_prefix;
    if (inst.opcode != 0x00) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    if (xmm_form) {
        *xmm_form = mandatory_prefix == 0x66;
    }

    decode_modrm_pshufb(ctx, &inst, code, code_size, &offset);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, inst.inst_size);
    ctx->last_inst_size = inst.inst_size;
    return inst;
}

inline void execute_pshufb_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const bool xmm_form = inst.mandatory_prefix == 0x66;

    if (xmm_form) {
        validate_pshufb_xmm_alignment(ctx, &inst);
        if (cpu_has_exception(ctx)) {
            return;
        }

        int dest = decode_pshufb_xmm_reg_index(ctx, inst.modrm);
        XMMRegister source = get_xmm128(ctx, dest);
        XMMRegister control = ((inst.modrm >> 6) & 0x03) == 0x03
            ? get_xmm128(ctx, decode_pshufb_xmm_rm_index(ctx, inst.modrm))
            : read_xmm_memory(ctx, inst.mem_address);
        set_xmm128(ctx, dest, apply_pshufb128(source, control));
        return;
    }

    int dest = (inst.modrm >> 3) & 0x07;
    uint64_t source = get_mm64(ctx, dest);
    uint64_t control = ((inst.modrm >> 6) & 0x03) == 0x03
        ? get_mm64(ctx, inst.modrm & 0x07)
        : read_memory_qword(ctx, inst.mem_address);
    set_mm64(ctx, dest, apply_pshufb64(source, control));
}

inline DecodedInstruction decode_pshufb_instruction_no_aux(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    bool xmm_form = false;
    DecodedInstruction inst = decode_pshufb_instruction(ctx, code, code_size, &xmm_form);
    if (cpu_has_exception(ctx)) {
        return inst;
    }
    inst.mandatory_prefix = xmm_form ? 0x66 : 0;
    return inst;
}

inline void execute_pshufb(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    bool xmm_form = false;
    DecodedInstruction inst = decode_pshufb_instruction(ctx, code, code_size, &xmm_form);
    if (cpu_has_exception(ctx)) {
        return;
    }
    inst.mandatory_prefix = xmm_form ? 0x66 : 0;
    execute_pshufb_with_decoded(ctx, &inst);
}

inline void execute_pshufb_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_pshufb_with_decoded(ctx, &dec->cached);
}
