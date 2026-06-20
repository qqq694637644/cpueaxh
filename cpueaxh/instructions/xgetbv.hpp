#pragma once

// instrusments/xgetbv.hpp - XGETBV instruction implementation

DecodedInstruction decode_xgetbv_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (has_lock_prefix) {
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
    if (inst.opcode != 0x01 || code[offset++] != 0xD0) {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_xgetbv_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* /*inst_ptr*/) {
    int cpuid_leaf1[4] = {};
    cpu_query_cpuid(cpuid_leaf1, 1, 0);
    if ((cpuid_leaf1[2] & (1 << 26)) == 0) {
        raise_ud_ctx(ctx);
        return;
    }
    if ((ctx->control_regs[4] & (1ULL << 18)) == 0) {
        raise_ud_ctx(ctx);
        return;
    }

    uint32_t xcr_index = get_reg32(ctx, REG_RCX);
    if (xcr_index != 0) {
        if (xcr_index != 1) {
            raise_gp_ctx(ctx, 0);
            return;
        }

        int cpuid_leaf_d1[4] = {};
        cpu_query_cpuid(cpuid_leaf_d1, 0x0D, 1);
        if ((cpuid_leaf_d1[0] & (1 << 2)) == 0) {
            raise_gp_ctx(ctx, 0);
            return;
        }
    }

    unsigned __int64 value = _xgetbv(xcr_index);
    set_reg32(ctx, REG_RAX, (uint32_t)value);
    set_reg32(ctx, REG_RDX, (uint32_t)(value >> 32));
}

void execute_xgetbv(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_xgetbv_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_xgetbv_with_decoded(ctx, &inst);
}

inline void execute_xgetbv_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_xgetbv_with_decoded(ctx, &dec->cached);
}
