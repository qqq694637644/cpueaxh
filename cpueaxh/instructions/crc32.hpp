// instructions/crc32.hpp - SSE4.2 CRC32 instruction implementation

static inline bool cpu_has_sse42_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 1, 0);
    return (cpu_info[2] & (1 << 20)) != 0;
}

static inline bool is_crc32_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 3 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F &&
        code[prefix_len + 1] == 0x38 &&
        (code[prefix_len + 2] == 0xF0 || code[prefix_len + 2] == 0xF1);
}

static inline int decode_crc32_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

static inline int decode_crc32_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

DecodedInstruction decode_crc32_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    const LegacyPrefixState prefixes = decode_legacy_prefixes(ctx, code, code_size, &offset);

    if (prefixes.has_lock || prefixes.has_f3 || !prefixes.has_f2) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (!cpu_has_sse42_feature()) {
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

    if (inst.opcode == 0xF0) {
        inst.operand_size = 8;
    }
    else if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }
    else {
        inst.operand_size = 32;
    }

    decode_modrm_common(ctx, &inst, code, code_size, &offset, true);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline uint64_t read_crc32_source(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    const uint8_t mod = (inst->modrm >> 6) & 0x03;
    const int rm = decode_crc32_rm_index(ctx, inst->modrm);
    if (mod == 3) {
        switch (inst->operand_size) {
        case 8:  return get_reg8(ctx, rm);
        case 16: return get_reg16(ctx, rm);
        case 32: return get_reg32(ctx, rm);
        case 64: return get_reg64(ctx, rm);
        default: raise_ud_ctx(ctx); return 0;
        }
    }

    switch (inst->operand_size) {
    case 8:  return read_memory_byte(ctx, inst->mem_address);
    case 16: return read_memory_word(ctx, inst->mem_address);
    case 32: return read_memory_dword(ctx, inst->mem_address);
    case 64: return read_memory_qword(ctx, inst->mem_address);
    default: raise_ud_ctx(ctx); return 0;
    }
}

static inline uint32_t crc32c_accumulate_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1) ^ ((crc & 1u) ? 0x82F63B78u : 0u);
    }
    return crc;
}

static inline uint32_t crc32c_accumulate_value(uint32_t crc, uint64_t value, int operand_size) {
    const int byte_count = operand_size / 8;
    for (int index = 0; index < byte_count; ++index) {
        crc = crc32c_accumulate_byte(crc, (uint8_t)((value >> (index * 8)) & 0xFFu));
    }
    return crc;
}

static inline void execute_crc32_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    const uint64_t source = read_crc32_source(ctx, &inst);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const int dest = decode_crc32_reg_index(ctx, inst.modrm);
    const uint32_t initial_crc = ctx->rex_w ? (uint32_t)get_reg64(ctx, dest) : get_reg32(ctx, dest);
    const uint32_t result = crc32c_accumulate_value(initial_crc, source, inst.operand_size);

    if (ctx->rex_w) {
        set_reg64(ctx, dest, result);
    }
    else {
        set_reg32(ctx, dest, result);
    }
}

void execute_crc32(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_crc32_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_crc32_with_decoded(ctx, &inst);
}
