#pragma once

// instrusments/cdq.hpp - CDQ/CQO instruction implementation

int decode_cdq_operand_size(CPU_CONTEXT* ctx) {
    if (ctx->cs.descriptor.long_mode) {
        if (ctx->rex_w) {
            return 64;
        }
        return ctx->operand_size_override ? 16 : 32;
    }
    return ctx->operand_size_override ? 16 : 32;
}

DecodedInstruction decode_cdq_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    if (inst.opcode != 0x99) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = decode_cdq_operand_size(ctx);
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    inst.inst_size = (int)offset;
    return inst;
}

inline void execute_cdq_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    if (inst.operand_size == 16) {
        uint16_t ax = get_reg16(ctx, REG_RAX);
        set_reg16(ctx, REG_RDX, (ax & 0x8000) ? 0xFFFF : 0x0000);
    }
    else if (inst.operand_size == 32) {
        int32_t eax = (int32_t)get_reg32(ctx, REG_RAX);
        set_reg32(ctx, REG_RDX, eax < 0 ? 0xFFFFFFFFU : 0x00000000U);
    }
    else {
        int64_t rax = (int64_t)get_reg64(ctx, REG_RAX);
        set_reg64(ctx, REG_RDX, rax < 0 ? 0xFFFFFFFFFFFFFFFFULL : 0x0000000000000000ULL);
    }
}

void execute_cdq(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_cdq_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_cdq_with_decoded(ctx, &inst);
}

inline void execute_cdq_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_cdq_with_decoded(ctx, &dec->cached);
}
