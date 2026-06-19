#pragma once

// instrusments/pause.hpp - PAUSE instruction implementation

DecodedInstruction decode_pause_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_pause_prefix = false;

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
        else if (prefix == 0xF3) {
            has_pause_prefix = true;
            offset++;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65 || prefix == 0xF2) {
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
    if (inst.opcode != 0x90) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix || !has_pause_prefix || ctx->rex_b) {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_pause_with_decoded(CPU_CONTEXT* /*ctx*/, const DecodedInstruction* /*inst_ptr*/) {
    // PAUSE is architecturally a hint with no observable side effects in this emulator.
}

void execute_pause(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    decode_pause_instruction(ctx, code, code_size);
}

inline void execute_pause_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_pause_with_decoded(ctx, &dec->cached);
}
