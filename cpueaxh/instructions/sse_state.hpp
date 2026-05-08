// instrusments/sse_state.hpp - LDMXCSR/STMXCSR/fence/CLFLUSH implementation

static void decode_sse_state_modrm(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
    if (*offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return;
    }

    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    uint8_t mod = (inst->modrm >> 6) & 0x03;
    uint8_t rm = inst->modrm & 0x07;

    if (mod != 3 && rm == 4 && inst->address_size != 16) {
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

    if (mod != 3) {
        inst->mem_address = get_effective_address(ctx, inst->modrm, &inst->sib, &inst->displacement, inst->address_size);
    }
}

DecodedInstruction decode_sse_state_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_simd_prefix = false;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

    while (offset < code_size) {
        uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            has_simd_prefix = true;
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
                 prefix == 0x64 || prefix == 0x65) {
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
    if (inst.opcode != 0xAE) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix || has_simd_prefix) {
        raise_ud_ctx(ctx);
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_sse_state_modrm(ctx, &inst, code, code_size, &offset);

    uint8_t mod = (inst.modrm >> 6) & 0x03;
    uint8_t reg = (inst.modrm >> 3) & 0x07;
    if (reg <= 3) {
        if (mod == 3) {
            raise_ud_ctx(ctx);
        }
    }
    else if (reg == 5) {
        if (inst.modrm != 0xE8) {
            raise_ud_ctx(ctx);
        }
    }
    else if (reg == 6) {
        if (inst.modrm != 0xF0) {
            raise_ud_ctx(ctx);
        }
    }
    else if (reg == 7) {
        if (mod == 3 && inst.modrm != 0xF8) {
            raise_ud_ctx(ctx);
        }
    }
    else {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static void sse_state_validate_mxcsr(CPU_CONTEXT* ctx, uint32_t value) {
    if ((value & 0xFFFF0000U) != 0) {
        raise_gp_ctx(ctx, 0);
        return;
    }
}

static void sse_state_validate_fx_area(CPU_CONTEXT* ctx, uint64_t address) {
    if ((address & 0x0FULL) != 0) {
        raise_gp_ctx(ctx, 0);
        return;
    }
}

static constexpr size_t kSseStateFxImageSize = 0x1A0;

static void sse_state_pack_fxsave(CPU_CONTEXT* ctx, uint8_t bytes[kSseStateFxImageSize]) {
    cpu_store_u16_le(bytes + 0x00, 0x037FU);
    cpu_store_u16_le(bytes + 0x02, 0x0000U);
    bytes[0x04] = 0x00U;
    bytes[0x05] = 0x00U;
    cpu_store_u16_le(bytes + 0x06, 0x0000U);
    cpu_store_u64_le(bytes + 0x08, 0);
    cpu_store_u64_le(bytes + 0x10, 0);
    cpu_store_u32_le(bytes + 0x18, ctx->mxcsr & 0x0000FFFFU);
    cpu_store_u32_le(bytes + 0x1C, 0x0000FFFFU);

    for (int index = 0; index < 8; ++index) {
        const size_t register_offset = 0x20 + static_cast<size_t>(index) * 0x10;
        cpu_store_u64_le(bytes + register_offset, ctx->mm[index]);
        cpu_store_u64_le(bytes + register_offset + 0x08, 0);
    }

    for (int index = 0; index < 16; ++index) {
        const XMMRegister value = get_xmm128(ctx, index);
        const size_t register_offset = 0xA0 + static_cast<size_t>(index) * 0x10;
        cpu_store_u64_le(bytes + register_offset, value.low);
        cpu_store_u64_le(bytes + register_offset + 0x08, value.high);
    }
}

static void sse_state_notify_fxsave_write_hooks(CPU_CONTEXT* ctx, uint64_t address, const uint8_t bytes[kSseStateFxImageSize]) {
    if (!cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        return;
    }

    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x00, 2, cpu_load_u16_le(bytes + 0x00));
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x02, 2, cpu_load_u16_le(bytes + 0x02));
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x04, 1, bytes[0x04]);
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x05, 1, bytes[0x05]);
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x06, 2, cpu_load_u16_le(bytes + 0x06));
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x08, 8, cpu_load_u64_le(bytes + 0x08));
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x10, 8, cpu_load_u64_le(bytes + 0x10));
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x18, 4, cpu_load_u32_le(bytes + 0x18));
    cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address + 0x1C, 4, cpu_load_u32_le(bytes + 0x1C));

    for (int index = 0; index < 8; ++index) {
        const uint64_t register_address = address + 0x20 + static_cast<uint64_t>(index) * 0x10;
        const size_t register_offset = 0x20 + static_cast<size_t>(index) * 0x10;
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, register_address, 8, cpu_load_u64_le(bytes + register_offset));
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, register_address + 0x08, 8, cpu_load_u64_le(bytes + register_offset + 0x08));
    }

    for (int index = 0; index < 16; ++index) {
        const uint64_t register_address = address + 0xA0 + static_cast<uint64_t>(index) * 0x10;
        const size_t register_offset = 0xA0 + static_cast<size_t>(index) * 0x10;
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, register_address, 8, cpu_load_u64_le(bytes + register_offset));
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, register_address + 0x08, 8, cpu_load_u64_le(bytes + register_offset + 0x08));
    }
}

static void sse_state_notify_fxrstor_read_hooks(CPU_CONTEXT* ctx, uint64_t address, const uint8_t bytes[kSseStateFxImageSize]) {
    if (!cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_READ)) {
        return;
    }

    for (int index = 0; index < 8; ++index) {
        const uint64_t register_address = address + 0x20 + static_cast<uint64_t>(index) * 0x10;
        const size_t register_offset = 0x20 + static_cast<size_t>(index) * 0x10;
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_READ, register_address, 8, cpu_load_u64_le(bytes + register_offset));
    }
    for (int index = 0; index < 16; ++index) {
        const uint64_t register_address = address + 0xA0 + static_cast<uint64_t>(index) * 0x10;
        const size_t register_offset = 0xA0 + static_cast<size_t>(index) * 0x10;
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_READ, register_address, 8, cpu_load_u64_le(bytes + register_offset));
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_READ, register_address + 0x08, 8, cpu_load_u64_le(bytes + register_offset + 0x08));
    }
}

static void sse_state_write_fxsave(CPU_CONTEXT* ctx, uint64_t address) {
    sse_state_validate_fx_area(ctx, address);
    if (cpu_has_exception(ctx)) {
        return;
    }

    uint8_t bytes[kSseStateFxImageSize] = {};
    uint8_t* byte_ptrs[kSseStateFxImageSize] = {};
    sse_state_pack_fxsave(ctx, bytes);
    if (!cpu_resolve_linear_write_byte_ptrs(ctx, address, sizeof(bytes), byte_ptrs, cpu_load_u16_le(bytes))) {
        return;
    }
    sse_state_notify_fxsave_write_hooks(ctx, address, bytes);
    cpu_commit_linear_write_bytes(bytes, sizeof(bytes), byte_ptrs);
}

static void sse_state_read_fxrstor(CPU_CONTEXT* ctx, uint64_t address) {
    sse_state_validate_fx_area(ctx, address);
    if (cpu_has_exception(ctx)) {
        return;
    }

    uint8_t bytes[kSseStateFxImageSize] = {};
    if (!cpu_read_linear_bytes_large(ctx, address, bytes, sizeof(bytes))) {
        return;
    }
    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_READ)) {
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_READ, address + 0x18, 4, cpu_load_u32_le(bytes + 0x18));
    }

    const uint32_t Mxcsr = cpu_load_u32_le(bytes + 0x18);
    sse_state_validate_mxcsr(ctx, Mxcsr);
    if (cpu_has_exception(ctx)) {
        return;
    }

    sse_state_notify_fxrstor_read_hooks(ctx, address, bytes);
    ctx->mxcsr = Mxcsr & 0x0000FFFFU;
    for (int index = 0; index < 8; ++index) {
        ctx->mm[index] = cpu_load_u64_le(bytes + 0x20 + static_cast<size_t>(index) * 0x10);
    }

    for (int index = 0; index < 16; ++index) {
        XMMRegister value = {};
        const size_t register_offset = 0xA0 + static_cast<size_t>(index) * 0x10;
        value.low = cpu_load_u64_le(bytes + register_offset);
        value.high = cpu_load_u64_le(bytes + register_offset + 0x08);
        set_xmm128(ctx, index, value);
    }
}

void execute_sse_state(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_sse_state_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    uint8_t reg = (inst.modrm >> 3) & 0x07;

    if (reg == 0) {
        sse_state_write_fxsave(ctx, inst.mem_address);
        return;
    }

    if (reg == 1) {
        sse_state_read_fxrstor(ctx, inst.mem_address);
        return;
    }

    if (reg == 2) {
        uint32_t value = read_memory_dword(ctx, inst.mem_address);
        sse_state_validate_mxcsr(ctx, value);
        ctx->mxcsr = value & 0x0000FFFFU;
        return;
    }

    if (reg == 3) {
        write_memory_dword(ctx, inst.mem_address, ctx->mxcsr & 0x0000FFFFU);
        return;
    }

    if (inst.modrm == 0xE8 || inst.modrm == 0xF0 || inst.modrm == 0xF8) {
        return;
    }

    if (reg == 7 && ((inst.modrm >> 6) & 0x03) != 3) {
        return;
    }

    raise_ud_ctx(ctx);
}
