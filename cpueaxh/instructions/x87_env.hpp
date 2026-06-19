#pragma once

// instructions/x87_env.hpp - FLDENV/FNSTENV/FSTENV implementation

static void x87_env_write16(CPU_CONTEXT* ctx, uint64_t address) {
    uint8_t bytes[14] = {};
    uint8_t* byte_ptrs[14] = {};
    const uint16_t values[7] = {
        ctx->x87_control_word,
        ctx->x87_status_word,
        ctx->x87_tag_word,
        static_cast<uint16_t>(ctx->x87_instruction_pointer & 0xFFFFu),
        ctx->x87_last_opcode,
        static_cast<uint16_t>(ctx->x87_data_pointer & 0xFFFFu),
        0
    };

    for (int index = 0; index < 7; ++index) {
        cpu_store_u16_le(bytes + index * 2, values[index]);
    }
    if (!cpu_resolve_linear_write_byte_ptrs(ctx, address, sizeof(bytes), byte_ptrs, values[0])) {
        return;
    }
    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        for (int index = 0; index < 7; ++index) {
            cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + static_cast<uint64_t>(index * 2), 2, values[index]);
        }
    }
    cpu_commit_linear_write_bytes(bytes, sizeof(bytes), byte_ptrs);
}

static void x87_env_write32(CPU_CONTEXT* ctx, uint64_t address) {
    uint8_t bytes[28] = {};
    uint8_t* byte_ptrs[28] = {};
    const uint32_t values[7] = {
        ctx->x87_control_word,
        ctx->x87_status_word,
        ctx->x87_tag_word,
        static_cast<uint32_t>(ctx->x87_instruction_pointer & 0xFFFFFFFFu),
        static_cast<uint32_t>(ctx->x87_last_opcode) << 16,
        static_cast<uint32_t>(ctx->x87_data_pointer & 0xFFFFFFFFu),
        0
    };

    for (int index = 0; index < 7; ++index) {
        cpu_store_u32_le(bytes + index * 4, values[index]);
    }
    if (!cpu_resolve_linear_write_byte_ptrs(ctx, address, sizeof(bytes), byte_ptrs, values[0])) {
        return;
    }
    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        for (int index = 0; index < 7; ++index) {
            cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + static_cast<uint64_t>(index * 4), 4, values[index]);
        }
    }
    cpu_commit_linear_write_bytes(bytes, sizeof(bytes), byte_ptrs);
}

static void x87_env_read16(CPU_CONTEXT* ctx, uint64_t address) {
    uint8_t bytes[14] = {};
    if (!cpu_read_linear_bytes(ctx, address, bytes, sizeof(bytes))) {
        return;
    }
    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_READ)) {
        for (int index = 0; index < 7; ++index) {
            cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_READ, address + static_cast<uint64_t>(index * 2), 2, cpu_load_u16_le(bytes + index * 2));
        }
    }
    ctx->x87_control_word = cpu_load_u16_le(bytes + 0x00);
    ctx->x87_status_word = cpu_load_u16_le(bytes + 0x02);
    ctx->x87_tag_word = cpu_load_u16_le(bytes + 0x04);
    ctx->x87_instruction_pointer = cpu_load_u16_le(bytes + 0x06);
    ctx->x87_last_opcode = cpu_load_u16_le(bytes + 0x08);
    ctx->x87_data_pointer = cpu_load_u16_le(bytes + 0x0A);
}

static void x87_env_read32(CPU_CONTEXT* ctx, uint64_t address) {
    uint8_t bytes[28] = {};
    if (!cpu_read_linear_bytes(ctx, address, bytes, sizeof(bytes))) {
        return;
    }
    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_READ)) {
        for (int index = 0; index < 7; ++index) {
            cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_READ, address + static_cast<uint64_t>(index * 4), 4, cpu_load_u32_le(bytes + index * 4));
        }
    }
    ctx->x87_control_word = static_cast<uint16_t>(cpu_load_u32_le(bytes + 0x00) & 0xFFFFu);
    ctx->x87_status_word = static_cast<uint16_t>(cpu_load_u32_le(bytes + 0x04) & 0xFFFFu);
    ctx->x87_tag_word = static_cast<uint16_t>(cpu_load_u32_le(bytes + 0x08) & 0xFFFFu);
    ctx->x87_instruction_pointer = cpu_load_u32_le(bytes + 0x0C);
    ctx->x87_last_opcode = static_cast<uint16_t>((cpu_load_u32_le(bytes + 0x10) >> 16) & 0xFFFFu);
    ctx->x87_data_pointer = cpu_load_u32_le(bytes + 0x14);
}

static DecodedInstruction decode_x87_env_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_rep_prefix = false;
    bool has_wait_prefix = false;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66) {
            ctx->operand_size_override = true;
            ++offset;
        }
        else if (prefix == 0xF2 || prefix == 0xF3) {
            has_rep_prefix = true;
            ++offset;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            ++offset;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            ctx->rex_present = true;
            ctx->rex_w = (prefix >> 3) & 1;
            ctx->rex_r = (prefix >> 2) & 1;
            ctx->rex_x = (prefix >> 1) & 1;
            ctx->rex_b = prefix & 1;
            ++offset;
        }
        else if (prefix == 0xF0) {
            has_lock_prefix = true;
            ++offset;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65) {
            ++offset;
        }
        else {
            break;
        }
    }

    if (offset < code_size && code[offset] == 0x9B) {
        has_wait_prefix = true;
        ++offset;
    }

    if (offset >= code_size || code[offset++] != 0xD9) {
        raise_ud_ctx(ctx);
        return inst;
    }
    if (has_lock_prefix || has_rep_prefix) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
        inst.operand_size = ctx->operand_size_override ? 16 : 32;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
        inst.operand_size = ctx->operand_size_override ? 16 : (ctx->cs.descriptor.db ? 32 : 16);
    }

    decode_x87_control_modrm(ctx, &inst, code, code_size, &offset);
    const uint8_t mod = (inst.modrm >> 6) & 0x03;
    const uint8_t reg = (inst.modrm >> 3) & 0x07;
    if ((reg != 4 && reg != 6) || mod == 3) {
        raise_ud_ctx(ctx);
        return inst;
    }

    const bool is_waiting_instruction = has_wait_prefix || reg == 4;
    if (!cpu_x87_check_device_available(ctx) || !cpu_x87_check_wait(ctx, is_waiting_instruction)) {
        return inst;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_x87_env(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_x87_env_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const uint8_t reg = (inst.modrm >> 3) & 0x07;
    if (reg == 4) {
        if (inst.operand_size == 16) {
            x87_env_read16(ctx, inst.mem_address);
        }
        else {
            x87_env_read32(ctx, inst.mem_address);
        }
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_x87_recompute_pending_exception(ctx);
        return;
    }

    if (inst.operand_size == 16) {
        x87_env_write16(ctx, inst.mem_address);
    }
    else {
        x87_env_write32(ctx, inst.mem_address);
    }
    if (cpu_has_exception(ctx)) {
        return;
    }

    ctx->x87_control_word = static_cast<uint16_t>(ctx->x87_control_word | 0x003Fu);
    cpu_x87_recompute_pending_exception(ctx);
}
