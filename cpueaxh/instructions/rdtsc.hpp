#pragma once

// instrusments/rdtsc.hpp - RDTSC instruction implementation

#include <intrin.h>

static inline bool rdtsc_tsd_forbidden(CPU_CONTEXT* ctx) {
    if (!ctx) {
        return false;
    }
    const bool protected_mode = (ctx->control_regs[0] & 0x1ULL) != 0;
    const bool tsd = (ctx->control_regs[4] & (1ULL << 2)) != 0;
    return protected_mode && tsd && ctx->cpl > 0;
}

DecodedInstruction decode_rdtsc_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0x31) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_rdtsc_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* /*inst_ptr*/) {
    if (rdtsc_tsd_forbidden(ctx)) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    unsigned __int64 tsc = __rdtsc();
    set_reg32(ctx, REG_RAX, (uint32_t)(tsc & 0xFFFFFFFFULL));
    set_reg32(ctx, REG_RDX, (uint32_t)(tsc >> 32));
}

void execute_rdtsc(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_rdtsc_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_rdtsc_with_decoded(ctx, &inst);
}

inline void execute_rdtsc_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_rdtsc_with_decoded(ctx, &dec->cached);
}
