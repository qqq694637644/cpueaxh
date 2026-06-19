#pragma once

// instrusments/pushf.hpp - PUSHF/POPF instruction implementation

static const uint64_t PUSHF_RF_BIT = (1ULL << 16);
static const uint64_t PUSHF_VM_BIT = (1ULL << 17);
static const uint64_t PUSHF_IF_BIT = (1ULL << 9);
static const uint64_t PUSHF_IOPL_MASK = (3ULL << 12);
static const uint64_t PUSHF_VIF_BIT = (1ULL << 19);
static const uint64_t PUSHF_VIP_BIT = (1ULL << 20);
static const uint64_t PUSHF_LOW16_WRITABLE_MASK = 0x7FD5ULL;
static const uint64_t PUSHF_EXTENDED_WRITABLE_MASK = 0x247FD5ULL;

int decode_pushf_operand_size(CPU_CONTEXT* ctx) {
    if (cpu_is_64bit_code(ctx)) {
        return ctx->operand_size_override ? 16 : 64;
    }
    return ctx->operand_size_override ? 16 : 32;
}

uint64_t get_pushf_image(CPU_CONTEXT* ctx, int operand_size) {
    uint64_t flags = ctx->rflags & ~(PUSHF_RF_BIT | PUSHF_VM_BIT);
    switch (operand_size) {
    case 16: return (uint16_t)flags;
    case 32: return (uint32_t)flags;
    case 64: return flags;
    default: raise_ud_ctx(ctx); return 0;
    }
}

void pushf_value(CPU_CONTEXT* ctx, int operand_size, uint64_t value) {
    switch (operand_size) {
    case 16:
        push_value16(ctx, (uint16_t)value);
        break;
    case 32:
        push_value32(ctx, (uint32_t)value);
        break;
    case 64:
        push_value64(ctx, value);
        break;
    default:
        raise_ud_ctx(ctx);
    }
}

uint64_t popf_value(CPU_CONTEXT* ctx, int operand_size) {
    switch (operand_size) {
    case 16: return pop_value16(ctx);
    case 32: return pop_value32(ctx);
    case 64: return pop_value64(ctx);
    default: raise_ud_ctx(ctx); return 0;
    }
}

void write_popf_flags(CPU_CONTEXT* ctx, int operand_size, uint64_t value) {
    uint64_t writable_mask = (operand_size == 16) ? PUSHF_LOW16_WRITABLE_MASK : PUSHF_EXTENDED_WRITABLE_MASK;
    uint64_t protected_mode = ctx->control_regs[0] & 1ULL;
    uint64_t iopl = (ctx->rflags >> 12) & 0x3ULL;

    if (protected_mode && ctx->cpl != 0) {
        writable_mask &= ~PUSHF_IOPL_MASK;
        if ((uint64_t)ctx->cpl > iopl) {
            writable_mask &= ~PUSHF_IF_BIT;
        }
    }

    writable_mask &= ~(PUSHF_RF_BIT | PUSHF_VM_BIT | PUSHF_VIF_BIT | PUSHF_VIP_BIT);

    switch (operand_size) {
    case 16:
        ctx->rflags = (ctx->rflags & ~writable_mask) | (value & writable_mask);
        break;
    case 32:
        ctx->rflags = (ctx->rflags & ~writable_mask) | (value & writable_mask);
        break;
    case 64:
        ctx->rflags = (ctx->rflags & ~writable_mask) | (value & writable_mask);
        break;
    default:
        raise_ud_ctx(ctx);
    }

    ctx->rflags &= ~PUSHF_RF_BIT;
}

DecodedInstruction decode_pushf_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;

    cpu_reset_prefix_state(ctx);

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
        else if (cpu_try_apply_rex_prefix(ctx, prefix)) {
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
    if (inst.opcode != 0x9C && inst.opcode != 0x9D) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = decode_pushf_operand_size(ctx);
    inst.address_size = get_stack_addr_size(ctx);
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_pushf_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    if (inst.opcode == 0x9C) {
        pushf_value(ctx, inst.operand_size, get_pushf_image(ctx, inst.operand_size));
        return;
    }

    uint64_t value = popf_value(ctx, inst.operand_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    write_popf_flags(ctx, inst.operand_size, value);
}

void execute_pushf(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_pushf_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_pushf_with_decoded(ctx, &inst);
}

inline void execute_pushf_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_pushf_with_decoded(ctx, &dec->cached);
}
