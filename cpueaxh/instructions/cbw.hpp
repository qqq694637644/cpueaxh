// instrusments/cbw.hpp - CBW/CWDE/CDQE instruction implementation

int decode_cbw_operand_size(CPU_CONTEXT* ctx) {
    if (ctx->cs.descriptor.long_mode) {
        if (ctx->rex_w) {
            return 64;
        }
        return ctx->operand_size_override ? 16 : 32;
    }
    return ctx->operand_size_override ? 16 : 32;
}

DecodedInstruction decode_cbw_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0x98) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = decode_cbw_operand_size(ctx);
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_cbw_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    if (inst.operand_size == 16) {
        int16_t value = (int8_t)get_reg8(ctx, REG_RAX);
        set_reg16(ctx, REG_RAX, (uint16_t)value);
    }
    else if (inst.operand_size == 32) {
        int32_t value = (int16_t)get_reg16(ctx, REG_RAX);
        set_reg32(ctx, REG_RAX, (uint32_t)value);
    }
    else {
        int64_t value = (int32_t)get_reg32(ctx, REG_RAX);
        set_reg64(ctx, REG_RAX, (uint64_t)value);
    }
}

void execute_cbw(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_cbw_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_cbw_with_decoded(ctx, &inst);
}

inline void execute_cbw_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_cbw_with_decoded(ctx, &dec->cached);
}
