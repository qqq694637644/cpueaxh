#pragma once

// instrusments/sse_misc.hpp - MOVNTPS/PREFETCHh/PREFETCHW/PREFETCHWT1 implementation

int decode_sse_misc_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

void decode_modrm_sse_misc(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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

    if (mod == 3) {
        raise_ud_ctx(ctx);
    }

    inst->mem_address = get_effective_address(ctx, inst->modrm, &inst->sib, &inst->displacement, inst->address_size);
}

DecodedInstruction decode_sse_misc_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    bool has_unsupported_prefix = false;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            has_unsupported_prefix = true;
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
            ctx->rex_b = prefix & 0x01;
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
    if (inst.opcode != 0x0D && inst.opcode != 0x18 && inst.opcode != 0x2B) {
        raise_ud_ctx(ctx);
    }

    if (has_unsupported_prefix) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_sse_misc(ctx, &inst, code, code_size, &offset, has_lock_prefix);

    uint8_t reg = (inst.modrm >> 3) & 0x07;
    if (inst.opcode == 0x18) {
        if (reg > 3) {
            raise_ud_ctx(ctx);
        }
    }
    else if (inst.opcode == 0x0D) {
        if (reg > 2) {
            raise_ud_ctx(ctx);
        }
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void validate_movntps_alignment(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if ((inst->mem_address & 0x0F) != 0) {
        raise_gp_ctx(ctx, 0);
        return;
    }
}

void execute_movntps_misc(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    int src = decode_sse_misc_xmm_reg_index(ctx, inst->modrm);
    validate_movntps_alignment(ctx, inst);
    write_xmm_memory(ctx, inst->mem_address, get_xmm128(ctx, src));
}

void execute_prefetch_misc(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    (void)ctx;
    (void)inst;
}

void execute_sse_misc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_sse_misc_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    switch (inst.opcode) {
    case 0x0D:
    case 0x18:
        execute_prefetch_misc(ctx, &inst);
        return;
    case 0x2B:
        execute_movntps_misc(ctx, &inst);
        return;
    default:
        raise_ud_ctx(ctx);
    }
}
