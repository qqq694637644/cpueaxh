// instrusments/ret.hpp - RET far instruction implementation

int decode_ret_far_operand_size(CPU_CONTEXT* ctx) {
    if (cpu_is_64bit_code(ctx)) {
        return ctx->operand_size_override ? 16 : 64;
    }
    return ctx->operand_size_override ? 16 : 32;
}

void ret_far_adjust_stack(CPU_CONTEXT* ctx, uint16_t adjustment) {
    int stack_addr_size = get_stack_addr_size(ctx);
    if (stack_addr_size == 64) {
        ctx->regs[REG_RSP] += adjustment;
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        esp += adjustment;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
    }
    else {
        uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFF);
        sp = (uint16_t)(sp + adjustment);
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | sp;
    }
}

void validate_ret_far_selector(CPU_CONTEXT* ctx, uint16_t selector, SegmentDescriptor* desc_out) {
    if (is_null_selector(selector)) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    SegmentDescriptor desc = load_descriptor_from_table(ctx, selector);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (!is_code_segment(desc.type)) {
        raise_gp_ctx(ctx, selector & 0xFFFC);
        return;
    }

    if (cpu_long_mode_active(ctx) && desc.long_mode && desc.db) {
        raise_gp_ctx(ctx, selector & 0xFFFC);
        return;
    }

    uint8_t rpl = selector & 0x03;
    if (rpl != ctx->cpl) {
        raise_gp_ctx(ctx, selector & 0xFFFC);
        return;
    }

    if (is_conforming_code_segment(desc.type)) {
        if (desc.dpl > ctx->cpl) {
            raise_gp_ctx(ctx, selector & 0xFFFC);
            return;
        }
    }
    else if (desc.dpl != ctx->cpl) {
        raise_gp_ctx(ctx, selector & 0xFFFC);
        return;
    }

    if (!desc.present) {
        raise_np_ctx(ctx, selector & 0xFFFC);
        return;
    }

    *desc_out = desc;
}

DecodedInstruction decode_ret_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    if (inst.opcode != 0xCA && inst.opcode != 0xCB) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = decode_ret_far_operand_size(ctx);
    inst.address_size = get_stack_addr_size(ctx);

    if (inst.opcode == 0xCA) {
        if (offset + 2 > code_size) {
            raise_gp_ctx(ctx, 0);
return inst;
        }
        inst.immediate = (uint16_t)code[offset] | ((uint16_t)code[offset + 1] << 8);
        inst.imm_size = 2;
        offset += 2;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_ret(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_ret_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    uint64_t new_rip;
    uint16_t new_selector;
    uint64_t stack_values[2] = {};
    uint64_t stack_after_pops = 0;
    if (inst.operand_size == 64) {
        if (!peek_stack_values(ctx, 8, 2, stack_values, &stack_after_pops)) {
            return;
        }
        new_rip = stack_values[0];
        new_selector = (uint16_t)stack_values[1];
    }
    else if (inst.operand_size == 32) {
        if (!peek_stack_values(ctx, 4, 2, stack_values, &stack_after_pops)) {
            return;
        }
        new_rip = stack_values[0];
        new_selector = (uint16_t)stack_values[1];
    }
    else {
        if (!peek_stack_values(ctx, 2, 2, stack_values, &stack_after_pops)) {
            return;
        }
        new_rip = stack_values[0];
        new_selector = (uint16_t)stack_values[1];
    }

    SegmentDescriptor new_desc = {};
    validate_ret_far_selector(ctx, new_selector, &new_desc);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (!cpu_validate_code_offset(ctx, new_rip, new_desc.long_mode ? 64 : inst.operand_size)) {
        return;
    }

    cpu_set_stack_pointer_value(ctx, inst.address_size, stack_after_pops);
    ret_far_adjust_stack(ctx, (uint16_t)inst.immediate);

    ctx->cs.selector = (new_selector & 0xFFFC) | ctx->cpl;
    ctx->cs.descriptor = new_desc;
    // Far-ret can switch back to a different code-segment width; mode-key
    // cache must be recomputed before the next instruction executes.
    ctx->cached_mode_key_valid = 0;

    if (inst.operand_size == 16) {
        ctx->rip = cpu_mask_code_offset(new_rip, 16);
    }
    else if (inst.operand_size == 32) {
        ctx->rip = cpu_mask_code_offset(new_rip, 32);
    }
    else {
        ctx->rip = new_rip;
    }
}

DecodedInstruction decode_iret_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
    if (inst.opcode != 0xCF) {
        raise_ud_ctx(ctx);
    }
    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = decode_ret_far_operand_size(ctx);
    inst.address_size = get_stack_addr_size(ctx);
    inst.inst_size = (int)offset;
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_iret(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_iret_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    uint64_t new_rip = 0;
    uint16_t new_selector = 0;
    uint64_t new_rflags = 0;
    uint64_t new_rsp = 0;
    uint16_t new_ss = 0;
    uint64_t stack_values[5] = {};
    uint64_t stack_after_pops = 0;
    if (inst.operand_size == 64) {
        if (!peek_stack_values(ctx, 8, 5, stack_values, &stack_after_pops)) {
            return;
        }
        new_rip = stack_values[0];
        new_selector = (uint16_t)stack_values[1];
        new_rflags = stack_values[2];
        new_rsp = stack_values[3];
        new_ss = (uint16_t)stack_values[4];
    }
    else if (inst.operand_size == 32) {
        if (!peek_stack_values(ctx, 4, 3, stack_values, &stack_after_pops)) {
            return;
        }
        new_rip = stack_values[0];
        new_selector = (uint16_t)stack_values[1];
        new_rflags = stack_values[2];
    }
    else {
        if (!peek_stack_values(ctx, 2, 3, stack_values, &stack_after_pops)) {
            return;
        }
        new_rip = stack_values[0];
        new_selector = (uint16_t)stack_values[1];
        new_rflags = stack_values[2];
    }

    SegmentDescriptor new_desc = {};
    validate_ret_far_selector(ctx, new_selector, &new_desc);
    if (cpu_has_exception(ctx)) {
        return;
    }
    const int target_operand_size = new_desc.long_mode ? 64 : inst.operand_size;
    if (!cpu_validate_code_offset(ctx, new_rip, target_operand_size)) {
        return;
    }

    ctx->cs.selector = (new_selector & 0xFFFC) | ctx->cpl;
    ctx->cs.descriptor = new_desc;
    ctx->rflags = new_rflags;
    ctx->cached_mode_key_valid = 0;
    if (target_operand_size == 16) {
        cpu_set_stack_pointer_value(ctx, inst.address_size, stack_after_pops);
        ctx->rip = cpu_mask_code_offset(new_rip, 16);
    }
    else if (target_operand_size == 32) {
        cpu_set_stack_pointer_value(ctx, inst.address_size, stack_after_pops);
        ctx->rip = cpu_mask_code_offset(new_rip, 32);
    }
    else {
        ctx->rip = new_rip;
        ctx->regs[REG_RSP] = new_rsp;
        ctx->ss.selector = new_ss;
    }
}
