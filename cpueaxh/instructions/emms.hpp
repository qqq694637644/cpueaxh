#pragma once

// instructions/emms.hpp - EMMS instruction implementation

#include "instruction_common.hpp"
#include "x87_status.hpp"

static bool emms_check_device_available(CPU_CONTEXT* ctx) {
    const uint64_t cr0 = ctx->control_regs[REG_CR0];
    if ((cr0 & (1ull << 2)) != 0) {
        raise_ud_ctx(ctx);
        return false;
    }
    if ((cr0 & (1ull << 3)) != 0) {
        raise_nm_ctx(ctx);
        return false;
    }
    return true;
}

static DecodedInstruction decode_emms_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    bool has_lock_prefix = false;
    bool has_simd_prefix = false;
    size_t offset = x87_status_parse_prefixes(ctx, code, code_size, &has_lock_prefix, &has_simd_prefix);

    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset] != 0x0F || code[offset + 1] != 0x77) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (has_lock_prefix || has_simd_prefix) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (!emms_check_device_available(ctx) || !cpu_x87_check_wait(ctx, true)) {
        return inst;
    }

    offset += 2;
    inst.opcode = 0x77;
    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_emms(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    decode_emms_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    ctx->x87_tag_word = cpu_x87_default_tag_word();
}
