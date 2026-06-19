#pragma once

// instrusments/lahf.hpp - LAHF/SAHF instruction implementation

uint8_t get_ah(CPU_CONTEXT* ctx) {
    return (uint8_t)((ctx->regs[REG_RAX] >> 8) & 0xFF);
}

void set_ah(CPU_CONTEXT* ctx, uint8_t value) {
    ctx->regs[REG_RAX] = (ctx->regs[REG_RAX] & ~0xFF00ULL) | ((uint64_t)value << 8);
}

uint8_t make_lahf_value(CPU_CONTEXT* ctx) {
    uint8_t value = 0x02;
    if (ctx->rflags & RFLAGS_CF) value |= 0x01;
    if (ctx->rflags & RFLAGS_PF) value |= 0x04;
    if (ctx->rflags & RFLAGS_AF) value |= 0x10;
    if (ctx->rflags & RFLAGS_ZF) value |= 0x40;
    if (ctx->rflags & RFLAGS_SF) value |= 0x80;
    return value;
}

void apply_sahf_value(CPU_CONTEXT* ctx, uint8_t value) {
    set_flag(ctx, RFLAGS_CF, (value & 0x01) != 0);
    set_flag(ctx, RFLAGS_PF, (value & 0x04) != 0);
    set_flag(ctx, RFLAGS_AF, (value & 0x10) != 0);
    set_flag(ctx, RFLAGS_ZF, (value & 0x40) != 0);
    set_flag(ctx, RFLAGS_SF, (value & 0x80) != 0);
}

static inline bool cpu_has_lahf_sahf_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0x80000001U, 0);
    return (cpu_info[2] & 0x1) != 0;
}

DecodedInstruction decode_lahf_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
                 prefix == 0x64 || prefix == 0x65 ||
                 prefix == 0xF2 || prefix == 0xF3) {
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
    if (inst.opcode != 0x9E && inst.opcode != 0x9F) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }
    if (cpu_is_64bit_code(ctx) && !cpu_has_lahf_sahf_feature()) {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_lahf_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    if (inst.opcode == 0x9F) {
        set_ah(ctx, make_lahf_value(ctx));
        return;
    }

    apply_sahf_value(ctx, get_ah(ctx));
}

void execute_lahf(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_lahf_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_lahf_with_decoded(ctx, &inst);
}

inline void execute_lahf_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_lahf_with_decoded(ctx, &dec->cached);
}
