// instrusments/cmpxchg8b16b.hpp - CMPXCHG8B/CMPXCHG16B instruction implementation

int decode_cmpxchg8b16b_group(uint8_t modrm) {
    return (modrm >> 3) & 0x07;
}

static inline bool cpu_has_cmpxchg16b_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 1, 0);
    return (cpu_info[2] & (1 << 13)) != 0;
}

void decode_modrm_cmpxchg8b16b(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
    if (*offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return;
    }

    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    if (decode_cmpxchg8b16b_group(inst->modrm) != 1) {
        raise_ud_ctx(ctx);
    }

    uint8_t mod = (inst->modrm >> 6) & 0x03;
    uint8_t rm = inst->modrm & 0x07;

    if (mod == 3) {
        raise_ud_ctx(ctx);
    }

    if (rm == 4 && inst->address_size != 16) {
        if (*offset >= code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }
        inst->has_sib = true;
        inst->sib = code[(*offset)++];
    }

    if (mod == 0 && rm == 5) {
        inst->disp_size = (inst->address_size == 16) ? 2 : 4;
    }
    else if (mod == 0 && inst->has_sib && (inst->sib & 0x07) == 5) {
        inst->disp_size = 4;
    }
    else if (mod == 1) {
        inst->disp_size = 1;
    }
    else if (mod == 2) {
        inst->disp_size = (inst->address_size == 16) ? 2 : 4;
    }

    if (inst->disp_size > 0) {
        if (*offset + inst->disp_size > code_size) {
            raise_gp_ctx(ctx, 0);
return;
        }

        inst->displacement = 0;
        for (int index = 0; index < inst->disp_size; index++) {
            inst->displacement |= ((int32_t)code[(*offset)++]) << (index * 8);
        }

        if (inst->disp_size == 1) {
            inst->displacement = (int8_t)inst->displacement;
        }
        else if (inst->disp_size == 2) {
            inst->displacement = (int16_t)inst->displacement;
        }
    }

    inst->mem_address = get_effective_address(ctx, inst->modrm, &inst->sib, &inst->displacement, inst->address_size);
}

DecodedInstruction decode_cmpxchg8b16b_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    if (inst.opcode != 0xC7) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = ctx->rex_w ? 128 : 64;
    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_cmpxchg8b16b(ctx, &inst, code, code_size, &offset);

    if (inst.operand_size == 128 && (inst.mem_address & 0x0FULL) != 0) {
        raise_gp_ctx(ctx, 0);
return inst;
    }
    if (inst.operand_size == 128 && !cpu_has_cmpxchg16b_feature()) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    (void)has_lock_prefix;

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_cmpxchg8b_memory(CPU_CONTEXT* ctx, uint64_t mem_address) {
    uint64_t destination = read_memory_qword(ctx, mem_address);
    uint64_t compare_value = ((uint64_t)get_reg32(ctx, REG_RDX) << 32) | (uint64_t)get_reg32(ctx, REG_RAX);

    if (destination == compare_value) {
        uint64_t source_value = ((uint64_t)get_reg32(ctx, REG_RCX) << 32) | (uint64_t)get_reg32(ctx, REG_RBX);
        write_memory_qword(ctx, mem_address, source_value);
        set_flag(ctx, RFLAGS_ZF, true);
    }
    else {
        set_reg32(ctx, REG_RAX, (uint32_t)destination);
        set_reg32(ctx, REG_RDX, (uint32_t)(destination >> 32));
        set_flag(ctx, RFLAGS_ZF, false);
    }
}

void execute_cmpxchg16b_memory(CPU_CONTEXT* ctx, uint64_t mem_address) {
    XMMRegister destination = read_xmm_memory(ctx, mem_address);
    uint64_t compare_low = get_reg64(ctx, REG_RAX);
    uint64_t compare_high = get_reg64(ctx, REG_RDX);

    if (destination.low == compare_low && destination.high == compare_high) {
        XMMRegister source = {};
        source.low = get_reg64(ctx, REG_RBX);
        source.high = get_reg64(ctx, REG_RCX);
        write_xmm_memory(ctx, mem_address, source);
        set_flag(ctx, RFLAGS_ZF, true);
    }
    else {
        set_reg64(ctx, REG_RAX, destination.low);
        set_reg64(ctx, REG_RDX, destination.high);
        set_flag(ctx, RFLAGS_ZF, false);
    }
}

void execute_cmpxchg8b16b(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_cmpxchg8b16b_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (inst.operand_size == 128) {
        execute_cmpxchg16b_memory(ctx, inst.mem_address);
    }
    else if (inst.operand_size == 64) {
        execute_cmpxchg8b_memory(ctx, inst.mem_address);
    }
    else {
        raise_ud_ctx(ctx);
    }
}
