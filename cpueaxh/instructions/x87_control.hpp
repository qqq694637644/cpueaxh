#pragma once

// instructions/x87_control.hpp - FLDCW/FNSTCW/FSTCW implementation

static void decode_x87_control_modrm(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
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
        for (int index = 0; index < inst->disp_size; ++index) {
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

static DecodedInstruction decode_x87_control_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
        uint8_t prefix = code[offset];
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
    }

    if (has_lock_prefix || has_rep_prefix) {
        raise_ud_ctx(ctx);
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
    if ((reg != 5 && reg != 7) || mod == 3) {
        raise_ud_ctx(ctx);
    }

    if (cpu_has_exception(ctx)) {
        return inst;
    }

    const bool is_waiting_instruction = has_wait_prefix || reg == 5;
    if (!cpu_x87_check_device_available(ctx) || !cpu_x87_check_wait(ctx, is_waiting_instruction)) {
        return inst;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

void execute_x87_control(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_x87_control_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const uint8_t reg = (inst.modrm >> 3) & 0x07;
    if (reg == 5) {
        ctx->x87_control_word = read_memory_word(ctx, inst.mem_address);
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_x87_recompute_pending_exception(ctx);
        return;
    }

    write_memory_word(ctx, inst.mem_address, ctx->x87_control_word);
}
