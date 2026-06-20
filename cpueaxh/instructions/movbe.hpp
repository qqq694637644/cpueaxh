#pragma once

// instructions/movbe.hpp - MOVBE instruction implementation

static inline bool cpu_has_movbe_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 1, 0);
    return (cpu_info[2] & (1 << 22)) != 0;
}

static inline bool is_movbe_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F &&
        code[prefix_len + 1] == 0x38 &&
        (code[prefix_len + 2] == 0xF0 || code[prefix_len + 2] == 0xF1);
}

static inline int decode_movbe_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

static inline int decode_movbe_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

DecodedInstruction decode_movbe_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    const LegacyPrefixState prefixes = decode_legacy_prefixes(ctx, code, code_size, &offset);

    if (prefixes.has_lock || prefixes.has_f2 || prefixes.has_f3) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (!cpu_has_movbe_feature()) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (offset + 4 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    if (code[offset++] != 0x0F || code[offset++] != 0x38) {
        raise_ud_ctx(ctx);
        return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0xF0 && inst.opcode != 0xF1) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }
    else {
        inst.operand_size = 32;
    }

    decode_modrm_common(ctx, &inst, code, code_size, &offset, false);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline uint16_t movbe_swap16(uint16_t value) {
    return (uint16_t)((value >> 8) | (value << 8));
}

static inline uint32_t movbe_swap32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
        ((value & 0x0000FF00u) << 8) |
        ((value & 0x00FF0000u) >> 8) |
        ((value & 0xFF000000u) >> 24);
}

static inline uint64_t movbe_swap64(uint64_t value) {
    return ((uint64_t)movbe_swap32((uint32_t)value) << 32) |
        (uint64_t)movbe_swap32((uint32_t)(value >> 32));
}

static inline uint64_t read_movbe_memory(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    switch (inst->operand_size) {
    case 16: return movbe_swap16(read_memory_word(ctx, inst->mem_address));
    case 32: return movbe_swap32(read_memory_dword(ctx, inst->mem_address));
    case 64: return movbe_swap64(read_memory_qword(ctx, inst->mem_address));
    default: raise_ud_ctx(ctx); return 0;
    }
}

static inline uint64_t read_movbe_reg(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    const int reg = decode_movbe_reg_index(ctx, inst->modrm);
    switch (inst->operand_size) {
    case 16: return get_reg16(ctx, reg);
    case 32: return get_reg32(ctx, reg);
    case 64: return get_reg64(ctx, reg);
    default: raise_ud_ctx(ctx); return 0;
    }
}

static inline void write_movbe_reg(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint64_t value) {
    const int reg = decode_movbe_reg_index(ctx, inst->modrm);
    switch (inst->operand_size) {
    case 16: set_reg16(ctx, reg, (uint16_t)value); break;
    case 32: set_reg32(ctx, reg, (uint32_t)value); break;
    case 64: set_reg64(ctx, reg, value); break;
    default: raise_ud_ctx(ctx); break;
    }
}

static inline void write_movbe_memory(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint64_t value) {
    switch (inst->operand_size) {
    case 16: write_memory_word(ctx, inst->mem_address, movbe_swap16((uint16_t)value)); break;
    case 32: write_memory_dword(ctx, inst->mem_address, movbe_swap32((uint32_t)value)); break;
    case 64: write_memory_qword(ctx, inst->mem_address, movbe_swap64(value)); break;
    default: raise_ud_ctx(ctx); break;
    }
}

static inline void execute_movbe_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    if (inst.opcode == 0xF0) {
        const uint64_t value = read_movbe_memory(ctx, &inst);
        if (!cpu_has_exception(ctx)) {
            write_movbe_reg(ctx, &inst, value);
        }
        return;
    }

    const uint64_t value = read_movbe_reg(ctx, &inst);
    if (!cpu_has_exception(ctx)) {
        write_movbe_memory(ctx, &inst, value);
    }
}

void execute_movbe(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_movbe_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_movbe_with_decoded(ctx, &inst);
}
