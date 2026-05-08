// instrusments/enter.hpp - ENTER instruction implementation

int decode_enter_operand_size(CPU_CONTEXT* ctx) {
    if (cpu_is_64bit_code(ctx)) {
        return ctx->operand_size_override ? 16 : 64;
    }
    return ctx->operand_size_override ? 16 : 32;
}

int decode_enter_stack_addr_size(CPU_CONTEXT* ctx) {
    if (cpu_is_64bit_code(ctx)) {
        return ctx->address_size_override ? 32 : 64;
    }

    int default_size = ctx->ss.descriptor.db ? 32 : 16;
    if (ctx->address_size_override) {
        return default_size == 32 ? 16 : 32;
    }
    return default_size;
}

uint64_t enter_get_stack_pointer_value(CPU_CONTEXT* ctx, int stack_addr_size) {
    if (stack_addr_size == 64) {
        return ctx->regs[REG_RSP];
    }
    if (stack_addr_size == 32) {
        return (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFFULL);
    }
    return (uint16_t)(ctx->regs[REG_RSP] & 0xFFFFULL);
}

void enter_set_stack_pointer_value(CPU_CONTEXT* ctx, int stack_addr_size, uint64_t value) {
    if (stack_addr_size == 64) {
        ctx->regs[REG_RSP] = value;
    }
    else if (stack_addr_size == 32) {
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | (uint32_t)value;
    }
    else {
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | (uint16_t)value;
    }
}

uint64_t enter_get_frame_pointer_address(CPU_CONTEXT* ctx, int stack_addr_size) {
    if (stack_addr_size == 64) {
        return ctx->regs[REG_RBP];
    }
    if (stack_addr_size == 32) {
        return (uint32_t)(ctx->regs[REG_RBP] & 0xFFFFFFFFULL);
    }
    return (uint16_t)(ctx->regs[REG_RBP] & 0xFFFFULL);
}

uint64_t enter_get_frame_pointer_address_from_value(uint64_t rbp_value, int stack_addr_size) {
    if (stack_addr_size == 64) {
        return rbp_value;
    }
    if (stack_addr_size == 32) {
        return (uint32_t)(rbp_value & 0xFFFFFFFFULL);
    }
    return (uint16_t)(rbp_value & 0xFFFFULL);
}

uint64_t enter_sub_stack_pointer_value(int stack_addr_size, uint64_t value, size_t delta) {
    if (stack_addr_size == 64) {
        return value - delta;
    }
    if (stack_addr_size == 32) {
        return (uint32_t)((uint32_t)value - (uint32_t)delta);
    }
    return (uint16_t)((uint16_t)value - (uint16_t)delta);
}

uint64_t enter_sub_frame_pointer_value(uint64_t rbp_value, int operand_size) {
    if (operand_size == 16) {
        const uint16_t bp = (uint16_t)((uint16_t)(rbp_value & 0xFFFFULL) - 2);
        return (rbp_value & ~0xFFFFULL) | bp;
    }
    if (operand_size == 32) {
        const uint32_t ebp = (uint32_t)((uint32_t)(rbp_value & 0xFFFFFFFFULL) - 4);
        return (rbp_value & ~0xFFFFFFFFULL) | ebp;
    }
    return rbp_value - 8;
}

uint64_t enter_read_frame_value(CPU_CONTEXT* ctx, int operand_size, int stack_addr_size, uint64_t rbp_value) {
    const uint64_t address = enter_get_frame_pointer_address_from_value(rbp_value, stack_addr_size);
    if (operand_size == 16) {
        return read_memory_word(ctx, address);
    }
    if (operand_size == 32) {
        return read_memory_dword(ctx, address);
    }
    if (stack_addr_size == 16) {
        raise_gp_ctx(ctx, 0);
        return 0;
    }
    return read_memory_qword(ctx, address);
}

void enter_push16(CPU_CONTEXT* ctx, int stack_addr_size, uint16_t value) {
    uint64_t sp = enter_get_stack_pointer_value(ctx, stack_addr_size);
    sp = enter_sub_stack_pointer_value(stack_addr_size, sp, 2);
    write_memory_word(ctx, sp, value);
    if (!cpu_has_exception(ctx)) {
        enter_set_stack_pointer_value(ctx, stack_addr_size, sp);
    }
}

void enter_push32(CPU_CONTEXT* ctx, int stack_addr_size, uint32_t value) {
    uint64_t sp = enter_get_stack_pointer_value(ctx, stack_addr_size);
    sp = enter_sub_stack_pointer_value(stack_addr_size, sp, 4);
    write_memory_dword(ctx, sp, value);
    if (!cpu_has_exception(ctx)) {
        enter_set_stack_pointer_value(ctx, stack_addr_size, sp);
    }
}

void enter_push64(CPU_CONTEXT* ctx, int stack_addr_size, uint64_t value) {
    if (stack_addr_size == 16) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    uint64_t sp = enter_get_stack_pointer_value(ctx, stack_addr_size);
    sp = enter_sub_stack_pointer_value(stack_addr_size, sp, 8);
    write_memory_qword(ctx, sp, value);
    if (!cpu_has_exception(ctx)) {
        enter_set_stack_pointer_value(ctx, stack_addr_size, sp);
    }
}

uint16_t enter_read_frame16(CPU_CONTEXT* ctx, int stack_addr_size) {
    return read_memory_word(ctx, enter_get_frame_pointer_address(ctx, stack_addr_size));
}

uint32_t enter_read_frame32(CPU_CONTEXT* ctx, int stack_addr_size) {
    return read_memory_dword(ctx, enter_get_frame_pointer_address(ctx, stack_addr_size));
}

uint64_t enter_read_frame64(CPU_CONTEXT* ctx, int stack_addr_size) {
    if (stack_addr_size == 16) {
        raise_gp_ctx(ctx, 0);
    }
    return read_memory_qword(ctx, enter_get_frame_pointer_address(ctx, stack_addr_size));
}

DecodedInstruction decode_enter_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;

    cpu_reset_prefix_state(ctx);

    bool has_lock_prefix = false;

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
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2 || prefix == 0xF3) {
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
    if (inst.opcode != 0xC8) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    if (offset + 3 > code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    uint16_t alloc_size = (uint16_t)code[offset] | ((uint16_t)code[offset + 1] << 8);
    uint8_t nesting_level = code[offset + 2];
    offset += 3;

    inst.immediate = (uint64_t)alloc_size | ((uint64_t)nesting_level << 16);
    inst.imm_size = 3;
    inst.operand_size = decode_enter_operand_size(ctx);
    inst.address_size = decode_enter_stack_addr_size(ctx);
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_enter_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    uint16_t alloc_size = (uint16_t)(inst.immediate & 0xFFFFULL);
    uint8_t nesting_level = (uint8_t)((inst.immediate >> 16) & 0x1FULL);
    const size_t value_size = (inst.operand_size == 16) ? 2 : (inst.operand_size == 32 ? 4 : 8);
    if (inst.address_size == 16 && value_size == 8) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    uint64_t push_values[32] = {};
    uint64_t push_addresses[32] = {};
    uint8_t push_bytes[32][8] = {};
    uint8_t* push_ptrs[32][8] = {};
    size_t push_count = 0;

    uint64_t current_sp = enter_get_stack_pointer_value(ctx, inst.address_size);
    uint64_t working_rbp = ctx->regs[REG_RBP];

    if (inst.operand_size == 16) {
        push_values[push_count] = get_reg16(ctx, REG_RBP);
    }
    else if (inst.operand_size == 32) {
        push_values[push_count] = get_reg32(ctx, REG_RBP);
    }
    else {
        push_values[push_count] = get_reg64(ctx, REG_RBP);
    }
    current_sp = enter_sub_stack_pointer_value(inst.address_size, current_sp, value_size);
    push_addresses[push_count++] = current_sp;

    const uint64_t frame_temp = current_sp;

    if (nesting_level > 0) {
        for (uint8_t level = 1; level < nesting_level; ++level) {
            working_rbp = enter_sub_frame_pointer_value(working_rbp, inst.operand_size);
            push_values[push_count] = enter_read_frame_value(ctx, inst.operand_size, inst.address_size, working_rbp);
            if (cpu_has_exception(ctx)) {
                return;
            }
            current_sp = enter_sub_stack_pointer_value(inst.address_size, current_sp, value_size);
            push_addresses[push_count++] = current_sp;
        }

        push_values[push_count] = frame_temp;
        current_sp = enter_sub_stack_pointer_value(inst.address_size, current_sp, value_size);
        push_addresses[push_count++] = current_sp;
    }

    for (size_t index = 0; index < push_count; ++index) {
        cpu_store_stack_value_le(push_bytes[index], value_size, push_values[index]);
        if (!cpu_resolve_stack_slot_write(ctx, push_addresses[index], value_size, push_values[index], push_ptrs[index])) {
            return;
        }
    }

    for (size_t index = 0; index < push_count; ++index) {
        cpu_commit_stack_slot_write(push_addresses[index], push_bytes[index], value_size, push_values[index], push_ptrs[index], ctx);
    }

    if (inst.operand_size == 16) {
        set_reg16(ctx, REG_RBP, (uint16_t)frame_temp);
    }
    else if (inst.operand_size == 32) {
        set_reg32(ctx, REG_RBP, (uint32_t)frame_temp);
    }
    else {
        set_reg64(ctx, REG_RBP, frame_temp);
    }

    current_sp = enter_sub_stack_pointer_value(inst.address_size, current_sp, alloc_size);
    enter_set_stack_pointer_value(ctx, inst.address_size, current_sp);
}

void execute_enter(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_enter_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_enter_with_decoded(ctx, &inst);
}

inline void execute_enter_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_enter_with_decoded(ctx, &dec->cached);
}
