#pragma once

// instrusments/idiv.hpp - IDIV instruction implementation

// --- IDIV internal helpers ---

int decode_idiv_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

uint64_t read_idiv_rm_operand(CPU_CONTEXT* ctx, uint8_t modrm, uint64_t mem_addr, int operand_size) {
    uint8_t mod = (modrm >> 6) & 0x03;
    int rm = decode_idiv_rm_index(ctx, modrm);

    if (mod == 3) {
        switch (operand_size) {
        case 8:  return get_reg8(ctx, rm);
        case 16: return get_reg16(ctx, rm);
        case 32: return get_reg32(ctx, rm);
        case 64: return get_reg64(ctx, rm);
        default: raise_ud_ctx(ctx); return 0;
        }
    }

    switch (operand_size) {
    case 8:  return read_memory_byte(ctx, mem_addr);
    case 16: return read_memory_word(ctx, mem_addr);
    case 32: return read_memory_dword(ctx, mem_addr);
    case 64: return read_memory_qword(ctx, mem_addr);
    default: raise_ud_ctx(ctx); return 0;
    }
}

bool idiv_quotient_overflows(uint64_t abs_quotient, bool negative, int bits) {
    uint64_t max_negative = (bits == 64) ? 0x8000000000000000ULL : (1ULL << (bits - 1));
    uint64_t max_positive = max_negative - 1;

    if (negative) {
        return abs_quotient > max_negative;
    }
    return abs_quotient > max_positive;
}

uint64_t pack_signed_value(uint64_t abs_value, bool negative, int bits) {
    uint64_t mask = (bits == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bits) - 1ULL);
    if (!negative || abs_value == 0) {
        return abs_value & mask;
    }
    return ((~abs_value) + 1ULL) & mask;
}

void negate_u128(uint64_t high, uint64_t low, uint64_t* out_high, uint64_t* out_low) {
    uint64_t neg_low = (~low) + 1ULL;
    uint64_t neg_high = (~high) + (neg_low == 0 ? 1ULL : 0ULL);
    *out_high = neg_high;
    *out_low = neg_low;
}

// --- IDIV execution helpers ---

// F6 /7 - IDIV r/m8
void idiv_rm8(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    uint8_t divisor_bits = (uint8_t)read_idiv_rm_operand(ctx, modrm, mem_addr, 8);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (divisor_bits == 0) {
        raise_de_ctx(ctx);
        return;
    }

    uint16_t dividend_bits = get_reg16(ctx, REG_RAX);
    bool dividend_negative = ((int16_t)dividend_bits) < 0;
    bool divisor_negative = ((int8_t)divisor_bits) < 0;

    uint16_t abs_dividend = dividend_negative ? (uint16_t)(~dividend_bits + 1U) : dividend_bits;
    uint8_t abs_divisor = divisor_negative ? (uint8_t)(~divisor_bits + 1U) : divisor_bits;

    uint16_t abs_quotient = abs_dividend / abs_divisor;
    uint8_t abs_remainder = (uint8_t)(abs_dividend % abs_divisor);
    bool quotient_negative = dividend_negative ^ divisor_negative;

    if (idiv_quotient_overflows(abs_quotient, quotient_negative, 8)) {
        raise_de_ctx(ctx);
        return;
    }

    uint8_t quotient_bits = (uint8_t)pack_signed_value(abs_quotient, quotient_negative, 8);
    uint8_t remainder_bits = (uint8_t)pack_signed_value(abs_remainder, dividend_negative, 8);
    set_reg16(ctx, REG_RAX, (uint16_t)(((uint16_t)remainder_bits << 8) | quotient_bits));
}

// F7 /7 - IDIV r/m16
void idiv_rm16(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    uint16_t divisor_bits = (uint16_t)read_idiv_rm_operand(ctx, modrm, mem_addr, 16);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (divisor_bits == 0) {
        raise_de_ctx(ctx);
        return;
    }

    uint32_t dividend_bits = ((uint32_t)get_reg16(ctx, REG_RDX) << 16) | (uint32_t)get_reg16(ctx, REG_RAX);
    bool dividend_negative = ((int32_t)dividend_bits) < 0;
    bool divisor_negative = ((int16_t)divisor_bits) < 0;

    uint32_t abs_dividend = dividend_negative ? (~dividend_bits + 1U) : dividend_bits;
    uint16_t abs_divisor = divisor_negative ? (uint16_t)(~divisor_bits + 1U) : divisor_bits;

    uint32_t abs_quotient = abs_dividend / abs_divisor;
    uint16_t abs_remainder = (uint16_t)(abs_dividend % abs_divisor);
    bool quotient_negative = dividend_negative ^ divisor_negative;

    if (idiv_quotient_overflows(abs_quotient, quotient_negative, 16)) {
        raise_de_ctx(ctx);
        return;
    }

    set_reg16(ctx, REG_RAX, (uint16_t)pack_signed_value(abs_quotient, quotient_negative, 16));
    set_reg16(ctx, REG_RDX, (uint16_t)pack_signed_value(abs_remainder, dividend_negative, 16));
}

// F7 /7 - IDIV r/m32
void idiv_rm32(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    uint32_t divisor_bits = (uint32_t)read_idiv_rm_operand(ctx, modrm, mem_addr, 32);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (divisor_bits == 0) {
        raise_de_ctx(ctx);
        return;
    }

    uint64_t dividend_bits = ((uint64_t)get_reg32(ctx, REG_RDX) << 32) | (uint64_t)get_reg32(ctx, REG_RAX);
    bool dividend_negative = ((int64_t)dividend_bits) < 0;
    bool divisor_negative = ((int32_t)divisor_bits) < 0;

    uint64_t abs_dividend = dividend_negative ? (~dividend_bits + 1ULL) : dividend_bits;
    uint32_t abs_divisor = divisor_negative ? (~divisor_bits + 1U) : divisor_bits;

    uint64_t abs_quotient = abs_dividend / abs_divisor;
    uint32_t abs_remainder = (uint32_t)(abs_dividend % abs_divisor);
    bool quotient_negative = dividend_negative ^ divisor_negative;

    if (idiv_quotient_overflows(abs_quotient, quotient_negative, 32)) {
        raise_de_ctx(ctx);
        return;
    }

    set_reg32(ctx, REG_RAX, (uint32_t)pack_signed_value(abs_quotient, quotient_negative, 32));
    set_reg32(ctx, REG_RDX, (uint32_t)pack_signed_value(abs_remainder, dividend_negative, 32));
}

// REX.W + F7 /7 - IDIV r/m64
void idiv_rm64(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t sib, int32_t disp, uint64_t mem_addr) {
    (void)sib;
    (void)disp;

    uint64_t divisor_bits = read_idiv_rm_operand(ctx, modrm, mem_addr, 64);
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (divisor_bits == 0) {
        raise_de_ctx(ctx);
        return;
    }

    uint64_t dividend_low = get_reg64(ctx, REG_RAX);
    uint64_t dividend_high = get_reg64(ctx, REG_RDX);
    bool dividend_negative = ((int64_t)dividend_high) < 0;
    bool divisor_negative = ((int64_t)divisor_bits) < 0;

    uint64_t abs_dividend_high = dividend_high;
    uint64_t abs_dividend_low = dividend_low;
    if (dividend_negative) {
        negate_u128(dividend_high, dividend_low, &abs_dividend_high, &abs_dividend_low);
    }

    uint64_t abs_divisor = divisor_negative ? (~divisor_bits + 1ULL) : divisor_bits;
    if (abs_dividend_high >= abs_divisor) {
        raise_de_ctx(ctx);
        return;
    }

    uint64_t abs_remainder = 0;
    uint64_t abs_quotient = cpueaxh_udiv128(abs_dividend_high, abs_dividend_low, abs_divisor, &abs_remainder);
    bool quotient_negative = dividend_negative ^ divisor_negative;

    if (idiv_quotient_overflows(abs_quotient, quotient_negative, 64)) {
        raise_de_ctx(ctx);
        return;
    }

    set_reg64(ctx, REG_RAX, pack_signed_value(abs_quotient, quotient_negative, 64));
    set_reg64(ctx, REG_RDX, pack_signed_value(abs_remainder, dividend_negative, 64));
}

// --- IDIV decode helpers ---

void decode_modrm_idiv(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool has_lock_prefix) {
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
        for (int i = 0; i < inst->disp_size; i++) {
            inst->displacement |= ((int32_t)code[(*offset)++]) << (i * 8);
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

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }
}

// --- IDIV instruction decoder ---

DecodedInstruction decode_idiv_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

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
    inst.operand_size = 32;
    if (ctx->rex_w) {
        inst.operand_size = 64;
    }
    else if (ctx->operand_size_override) {
        inst.operand_size = 16;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    switch (inst.opcode) {
    case 0xF6:
        inst.operand_size = 8;
        decode_modrm_idiv(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 7) {
            raise_ud_ctx(ctx);
        }
        break;

    case 0xF7:
        decode_modrm_idiv(ctx, &inst, code, code_size, &offset, has_lock_prefix);
        if (((inst.modrm >> 3) & 0x07) != 7) {
            raise_ud_ctx(ctx);
        }
        break;

    default:
        raise_ud_ctx(ctx);
    }

    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

// --- IDIV instruction executor ---

inline void execute_idiv_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* inst_ptr) {
    const DecodedInstruction& inst = *inst_ptr;

    switch (inst.opcode) {
    case 0xF6:
        idiv_rm8(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        break;

    case 0xF7:
        if (ctx->rex_w) {
            idiv_rm64(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else if (ctx->operand_size_override) {
            idiv_rm16(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        else {
            idiv_rm32(ctx, inst.modrm, inst.sib, inst.displacement, inst.mem_address);
        }
        break;
    }
}

void execute_idiv(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_idiv_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_idiv_with_decoded(ctx, &inst);
}

inline void execute_idiv_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_idiv_with_decoded(ctx, &dec->cached);
}
