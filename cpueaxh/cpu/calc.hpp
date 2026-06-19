#pragma once

// cpu/calc.hpp - CPU calculation functions (flags, arithmetic)

// Compute parity of the lowest byte: true if even number of set bits
bool calc_parity(uint8_t val) {
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return (val & 1) == 0;
}

// Set/clear a single flag bit
void set_flag(CPU_CONTEXT* ctx, uint64_t flag, bool value) {
    if (cpu_has_exception(ctx)) {
        return;
    }

    if (value) {
        ctx->rflags |= flag;
    }
    else {
        ctx->rflags &= ~flag;
    }
}

// Update OF, SF, ZF, AF, CF, PF after an ADD operation (8-bit)
void update_flags_add8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t src, uint16_t result) {
    uint8_t res8 = (uint8_t)result;
    set_flag(ctx, RFLAGS_CF, result > 0xFF);
    set_flag(ctx, RFLAGS_OF, ((dst ^ res8) & (src ^ res8) & 0x80) != 0);
    set_flag(ctx, RFLAGS_SF, (res8 & 0x80) != 0);
    set_flag(ctx, RFLAGS_ZF, res8 == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ res8) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity(res8));
}

// Update OF, SF, ZF, AF, CF, PF after an ADD operation (16-bit)
void update_flags_add16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t src, uint32_t result) {
    uint16_t res16 = (uint16_t)result;
    set_flag(ctx, RFLAGS_CF, result > 0xFFFF);
    set_flag(ctx, RFLAGS_OF, ((dst ^ res16) & (src ^ res16) & 0x8000) != 0);
    set_flag(ctx, RFLAGS_SF, (res16 & 0x8000) != 0);
    set_flag(ctx, RFLAGS_ZF, res16 == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ res16) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res16));
}

// Update OF, SF, ZF, AF, CF, PF after an ADD operation (32-bit)
void update_flags_add32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t src, uint64_t result) {
    uint32_t res32 = (uint32_t)result;
    set_flag(ctx, RFLAGS_CF, result > 0xFFFFFFFFULL);
    set_flag(ctx, RFLAGS_OF, ((dst ^ res32) & (src ^ res32) & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_SF, (res32 & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_ZF, res32 == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ res32) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res32));
}

// Update OF, SF, ZF, AF, CF, PF after an ADD operation (64-bit)
void update_flags_add64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t src, uint64_t result) {
    // CF: unsigned overflow
    set_flag(ctx, RFLAGS_CF, result < dst);
    set_flag(ctx, RFLAGS_OF, ((dst ^ result) & (src ^ result) & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ result) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update OF, SF, ZF, AF, CF, PF after a SUB operation (8-bit)
// DEST := DEST - SRC
void update_flags_sub8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t src, uint16_t result) {
    uint8_t res8 = (uint8_t)result;
    // CF: borrow occurred if dst < src (unsigned)
    set_flag(ctx, RFLAGS_CF, dst < src);
    // OF: signed overflow if signs of dst and src differ, and result sign differs from dst
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ res8) & 0x80) != 0);
    set_flag(ctx, RFLAGS_SF, (res8 & 0x80) != 0);
    set_flag(ctx, RFLAGS_ZF, res8 == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ res8) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity(res8));
}

// Update OF, SF, ZF, AF, CF, PF after a SUB operation (16-bit)
void update_flags_sub16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t src, uint32_t result) {
    uint16_t res16 = (uint16_t)result;
    set_flag(ctx, RFLAGS_CF, dst < src);
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ res16) & 0x8000) != 0);
    set_flag(ctx, RFLAGS_SF, (res16 & 0x8000) != 0);
    set_flag(ctx, RFLAGS_ZF, res16 == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ res16) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res16));
}

// Update OF, SF, ZF, AF, CF, PF after a SUB operation (32-bit)
void update_flags_sub32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t src, uint64_t result) {
    uint32_t res32 = (uint32_t)result;
    set_flag(ctx, RFLAGS_CF, dst < src);
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ res32) & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_SF, (res32 & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_ZF, res32 == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ res32) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res32));
}

// Update OF, SF, ZF, AF, CF, PF after a SUB operation (64-bit)
void update_flags_sub64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t src, uint64_t result) {
    set_flag(ctx, RFLAGS_CF, dst < src);
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ result) & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_AF, ((dst ^ src ^ result) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update OF, SF, ZF, AF, CF, PF after an ADC operation (8-bit)
void update_flags_adc8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t src, uint8_t carry_in, uint16_t result) {
    uint8_t res8 = (uint8_t)result;
    set_flag(ctx, RFLAGS_CF, result > 0xFF);
    set_flag(ctx, RFLAGS_OF, ((dst ^ res8) & (src ^ res8) & 0x80) != 0);
    set_flag(ctx, RFLAGS_SF, (res8 & 0x80) != 0);
    set_flag(ctx, RFLAGS_ZF, res8 == 0);
    set_flag(ctx, RFLAGS_AF, (((dst & 0x0F) + (src & 0x0F) + carry_in) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity(res8));
}

// Update OF, SF, ZF, AF, CF, PF after an ADC operation (16-bit)
void update_flags_adc16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t src, uint8_t carry_in, uint32_t result) {
    uint16_t res16 = (uint16_t)result;
    set_flag(ctx, RFLAGS_CF, result > 0xFFFF);
    set_flag(ctx, RFLAGS_OF, ((dst ^ res16) & (src ^ res16) & 0x8000) != 0);
    set_flag(ctx, RFLAGS_SF, (res16 & 0x8000) != 0);
    set_flag(ctx, RFLAGS_ZF, res16 == 0);
    set_flag(ctx, RFLAGS_AF, (((dst & 0x0F) + (src & 0x0F) + carry_in) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res16));
}

// Update OF, SF, ZF, AF, CF, PF after an ADC operation (32-bit)
void update_flags_adc32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t src, uint8_t carry_in, uint64_t result) {
    uint32_t res32 = (uint32_t)result;
    set_flag(ctx, RFLAGS_CF, result > 0xFFFFFFFFULL);
    set_flag(ctx, RFLAGS_OF, ((dst ^ res32) & (src ^ res32) & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_SF, (res32 & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_ZF, res32 == 0);
    set_flag(ctx, RFLAGS_AF, (((dst & 0x0F) + (src & 0x0F) + carry_in) & 0x10) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res32));
}

// Update OF, SF, ZF, AF, CF, PF after an ADC operation (64-bit)
void update_flags_adc64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t src, uint8_t carry_in, uint64_t result) {
    set_flag(ctx, RFLAGS_CF, result < dst || (carry_in && result == dst));
    set_flag(ctx, RFLAGS_OF, ((dst ^ result) & (src ^ result) & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_AF, (((dst & 0x0FULL) + (src & 0x0FULL) + carry_in) & 0x10ULL) != 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update OF, SF, ZF, AF, CF, PF after an SBB operation (8-bit)
void update_flags_sbb8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t src, uint8_t borrow_in, uint16_t result) {
    uint8_t res8 = (uint8_t)result;
    set_flag(ctx, RFLAGS_CF, dst < src || (borrow_in && dst == src));
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ res8) & 0x80) != 0);
    set_flag(ctx, RFLAGS_SF, (res8 & 0x80) != 0);
    set_flag(ctx, RFLAGS_ZF, res8 == 0);
    set_flag(ctx, RFLAGS_AF, (dst & 0x0F) < ((src & 0x0F) + borrow_in));
    set_flag(ctx, RFLAGS_PF, calc_parity(res8));
}

// Update OF, SF, ZF, AF, CF, PF after an SBB operation (16-bit)
void update_flags_sbb16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t src, uint8_t borrow_in, uint32_t result) {
    uint16_t res16 = (uint16_t)result;
    set_flag(ctx, RFLAGS_CF, dst < src || (borrow_in && dst == src));
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ res16) & 0x8000) != 0);
    set_flag(ctx, RFLAGS_SF, (res16 & 0x8000) != 0);
    set_flag(ctx, RFLAGS_ZF, res16 == 0);
    set_flag(ctx, RFLAGS_AF, (dst & 0x0F) < ((src & 0x0F) + borrow_in));
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res16));
}

// Update OF, SF, ZF, AF, CF, PF after an SBB operation (32-bit)
void update_flags_sbb32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t src, uint8_t borrow_in, uint64_t result) {
    uint32_t res32 = (uint32_t)result;
    set_flag(ctx, RFLAGS_CF, dst < src || (borrow_in && dst == src));
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ res32) & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_SF, (res32 & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_ZF, res32 == 0);
    set_flag(ctx, RFLAGS_AF, (dst & 0x0F) < ((src & 0x0F) + borrow_in));
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)res32));
}

// Update OF, SF, ZF, AF, CF, PF after an SBB operation (64-bit)
void update_flags_sbb64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t src, uint8_t borrow_in, uint64_t result) {
    set_flag(ctx, RFLAGS_CF, dst < src || (borrow_in && dst == src));
    set_flag(ctx, RFLAGS_OF, ((dst ^ src) & (dst ^ result) & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_AF, (dst & 0x0FULL) < ((src & 0x0FULL) + borrow_in));
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after a TEST operation (8-bit)
// AF is left undefined (unchanged). Result = SRC1 AND SRC2, discarded.
void update_flags_test8(CPU_CONTEXT* ctx, uint8_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x80) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity(result));
}

// Update CF, OF, SF, ZF, PF after a TEST operation (16-bit)
void update_flags_test16(CPU_CONTEXT* ctx, uint16_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after a TEST operation (32-bit)
void update_flags_test32(CPU_CONTEXT* ctx, uint32_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after a TEST operation (64-bit)
void update_flags_test64(CPU_CONTEXT* ctx, uint64_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after an AND operation (8-bit)
// CF=0, OF=0; AF undefined (unchanged). Identical semantics to TEST flags.
void update_flags_and8(CPU_CONTEXT* ctx, uint8_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x80) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity(result));
}

// Update CF, OF, SF, ZF, PF after an AND operation (16-bit)
void update_flags_and16(CPU_CONTEXT* ctx, uint16_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after an AND operation (32-bit)
void update_flags_and32(CPU_CONTEXT* ctx, uint32_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x80000000U) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after an AND operation (64-bit)
void update_flags_and64(CPU_CONTEXT* ctx, uint64_t result) {
    set_flag(ctx, RFLAGS_CF, false);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_SF, (result & 0x8000000000000000ULL) != 0);
    set_flag(ctx, RFLAGS_ZF, result == 0);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

// Update CF, OF, SF, ZF, PF after an OR operation (8-bit)
void update_flags_or8(CPU_CONTEXT* ctx, uint8_t result) {
    update_flags_and8(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an OR operation (16-bit)
void update_flags_or16(CPU_CONTEXT* ctx, uint16_t result) {
    update_flags_and16(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an OR operation (32-bit)
void update_flags_or32(CPU_CONTEXT* ctx, uint32_t result) {
    update_flags_and32(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an OR operation (64-bit)
void update_flags_or64(CPU_CONTEXT* ctx, uint64_t result) {
    update_flags_and64(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an XOR operation (8-bit)
void update_flags_xor8(CPU_CONTEXT* ctx, uint8_t result) {
    update_flags_and8(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an XOR operation (16-bit)
void update_flags_xor16(CPU_CONTEXT* ctx, uint16_t result) {
    update_flags_and16(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an XOR operation (32-bit)
void update_flags_xor32(CPU_CONTEXT* ctx, uint32_t result) {
    update_flags_and32(ctx, result);
}

// Update CF, OF, SF, ZF, PF after an XOR operation (64-bit)
void update_flags_xor64(CPU_CONTEXT* ctx, uint64_t result) {
    update_flags_and64(ctx, result);
}

// Update OF, SF, ZF, AF, PF after an INC operation (8-bit); CF is unchanged.
void update_flags_inc8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_add8(ctx, dst, 1, (uint16_t)result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after an INC operation (16-bit); CF is unchanged.
void update_flags_inc16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_add16(ctx, dst, 1, (uint32_t)result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after an INC operation (32-bit); CF is unchanged.
void update_flags_inc32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_add32(ctx, dst, 1, (uint64_t)result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after an INC operation (64-bit); CF is unchanged.
void update_flags_inc64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_add64(ctx, dst, 1, result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after a DEC operation (8-bit); CF is unchanged.
void update_flags_dec8(CPU_CONTEXT* ctx, uint8_t dst, uint8_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_sub8(ctx, dst, 1, (uint16_t)result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after a DEC operation (16-bit); CF is unchanged.
void update_flags_dec16(CPU_CONTEXT* ctx, uint16_t dst, uint16_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_sub16(ctx, dst, 1, (uint32_t)result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after a DEC operation (32-bit); CF is unchanged.
void update_flags_dec32(CPU_CONTEXT* ctx, uint32_t dst, uint32_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_sub32(ctx, dst, 1, (uint64_t)result);
    set_flag(ctx, RFLAGS_CF, carry);
}

// Update OF, SF, ZF, AF, PF after a DEC operation (64-bit); CF is unchanged.
void update_flags_dec64(CPU_CONTEXT* ctx, uint64_t dst, uint64_t result) {
    bool carry = (ctx->rflags & RFLAGS_CF) != 0;
    update_flags_sub64(ctx, dst, 1, result);
    set_flag(ctx, RFLAGS_CF, carry);
}

