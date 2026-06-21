#pragma once

// instructions/rdpmc.hpp - RDPMC instruction implementation.

#include "instruction_common.hpp"

static DecodedInstruction decode_rdpmc_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;

    cpu_reset_prefix_state(ctx);
    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66) {
            ctx->operand_size_override = true;
            ++offset;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            ++offset;
        }
        else if (cpu_try_apply_rex_prefix(ctx, prefix)) {
            ++offset;
        }
        else if (prefix == 0xF0) {
            has_lock_prefix = true;
            ++offset;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
            ++offset;
        }
        else {
            break;
        }
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    if (code[offset++] != 0x0F || code[offset++] != 0x33) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.opcode = 0x33;
    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_rdpmc_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* /*inst_ptr*/) {
    const bool protected_mode = (ctx->control_regs[REG_CR0] & 0x1ULL) != 0;
    const bool pce = (ctx->control_regs[REG_CR4] & (1ULL << 8)) != 0;
    if (protected_mode && ctx->cpl > 0 && !pce) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    // RDPMC exposes implementation-defined performance-monitoring state. Keep
    // the emulator deterministic and avoid executing host RDPMC in restricted
    // user-mode runners; escape callbacks can provide real counter values.
    (void)get_reg32(ctx, REG_RCX);
    set_reg32(ctx, REG_RAX, 0);
    set_reg32(ctx, REG_RDX, 0);
}

void execute_rdpmc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_rdpmc_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_rdpmc_with_decoded(ctx, &inst);
}

inline void execute_rdpmc_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_rdpmc_with_decoded(ctx, &dec->cached);
}
