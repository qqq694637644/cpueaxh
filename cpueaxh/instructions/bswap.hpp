#pragma once

// instrusments/bswap.hpp - BSWAP instruction implementation

int decode_bswap_reg_index(CPU_CONTEXT* ctx, uint8_t opcode) {
    int reg = opcode - 0xC8;
    if (ctx->rex_b) {
        reg |= 0x08;
    }
    return reg;
}

uint32_t byteswap32_bswap(uint32_t value) {
    return ((value & 0x000000FFU) << 24)
         | ((value & 0x0000FF00U) << 8)
         | ((value & 0x00FF0000U) >> 8)
         | ((value & 0xFF000000U) >> 24);
}

uint64_t byteswap64_bswap(uint64_t value) {
    return ((value & 0x00000000000000FFULL) << 56)
         | ((value & 0x000000000000FF00ULL) << 40)
         | ((value & 0x0000000000FF0000ULL) << 24)
         | ((value & 0x00000000FF000000ULL) << 8)
         | ((value & 0x000000FF00000000ULL) >> 8)
         | ((value & 0x0000FF0000000000ULL) >> 24)
         | ((value & 0x00FF000000000000ULL) >> 40)
         | ((value & 0xFF00000000000000ULL) >> 56);
}

DecodedInstruction decode_bswap_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0x0F) {
        raise_ud_ctx(ctx);
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    inst.opcode = code[offset++];
    if (inst.opcode < 0xC8 || inst.opcode > 0xCF) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.operand_size = ctx->rex_w ? 64 : 32;
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_bswap_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;
    int reg = decode_bswap_reg_index(ctx, inst.opcode);

    if (inst.operand_size == 64) {
        uint64_t value = get_reg64(ctx, reg);
        set_reg64(ctx, reg, byteswap64_bswap(value));
    }
    else {
        uint32_t value = get_reg32(ctx, reg);
        set_reg32(ctx, reg, byteswap32_bswap(value));
    }
}

void execute_bswap(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_bswap_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_bswap_with_decoded(ctx, &inst);
}

inline void execute_bswap_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_bswap_with_decoded(ctx, &dec->cached);
}
