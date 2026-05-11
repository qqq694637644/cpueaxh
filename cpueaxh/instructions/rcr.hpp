// instrusments/rcr.hpp - RCR instruction implementation

void update_flags_rcr(CPU_CONTEXT* ctx, uint64_t result, int operand_size, unsigned int count, bool carry_out) {
    if (count == 0) {
        return;
    }

    set_flag(ctx, RFLAGS_CF, carry_out);
    if (count == 1) {
        uint64_t sign_mask = get_rol_sign_mask(ctx, operand_size);
        bool msb = (result & sign_mask) != 0;
        bool next_msb = (result & (sign_mask >> 1)) != 0;
        set_flag(ctx, RFLAGS_OF, msb ^ next_msb);
    }
}

void rcr_rm(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr, int operand_size, uint8_t raw_count) {
    (void)sib;
    (void)disp;

    uint64_t mask = get_rol_operand_mask(ctx, operand_size);
    uint64_t sign_mask = get_rol_sign_mask(ctx, operand_size);
    unsigned int count = get_rcl_count(ctx, operand_size, raw_count);
    if (cpu_has_exception(ctx)) {
        return;
    }

    uint64_t result = read_rol_rm_operand(ctx, modrm, mem_addr, operand_size) & mask;
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (count == 0) {
        return;
    }

    bool carry_out = false;
    for (unsigned int i = 0; i < count; i++) {
        bool carry_in = (ctx->rflags & RFLAGS_CF) != 0;
        carry_out = (result & 0x1ULL) != 0;
        result = (result >> 1) | (carry_in ? sign_mask : 0ULL);
        result &= mask;
        set_flag(ctx, RFLAGS_CF, carry_out);
    }

    write_rol_rm_operand(ctx, modrm, mem_addr, operand_size, result);
    update_flags_rcr(ctx, result, operand_size, count, carry_out);
}

DecodedInstruction decode_rcr_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    switch (inst.opcode) {
    case 0xD0:
    case 0xD2:
    case 0xC0:
        inst.operand_size = 8;
        decode_modrm_rol(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 3) {
            raise_ud_ctx(ctx);
        }
        if (inst.opcode == 0xC0) {
            if (offset >= code_size) {
                raise_gp_ctx(ctx, 0);
                return inst;
            }
            inst.immediate = code[offset++];
            inst.imm_size = 1;
        }
        break;

    case 0xD1:
    case 0xD3:
    case 0xC1:
        decode_modrm_rol(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 3) {
            raise_ud_ctx(ctx);
        }
        if (inst.opcode == 0xC1) {
            if (offset >= code_size) {
                raise_gp_ctx(ctx, 0);
                return inst;
            }
            inst.immediate = code[offset++];
            inst.imm_size = 1;
        }
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_rcr_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    DecodedInstruction inst = *inst_ptr;
    uint8_t count = decode_rol_count(ctx, &inst);
    rcr_rm(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address, inst.operand_size, count);
}

void execute_rcr(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_rcr_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_rcr_with_decoded(ctx, &inst);
}

inline void execute_rcr_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_rcr_with_decoded(ctx, &dec->cached);
}
