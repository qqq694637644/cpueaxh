#pragma once

// instrusments/palignr.hpp - PALIGNR/VPALIGNR implementation

static inline bool is_palignr_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 4 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x3A && code[prefix_len + 2] == 0x0F;
}

static inline bool is_evex_palignr_instruction(const uint8_t* code, size_t code_size) {
    return code && code_size >= 7 && code[0] == 0x62 && code[4] == 0x0F;
}

static inline void palignr_qword_to_bytes(uint64_t value, uint8_t bytes[8]) {
    for (int index = 0; index < 8; ++index) {
        bytes[index] = (uint8_t)((value >> (index * 8)) & 0xFFu);
    }
}

static inline uint64_t palignr_bytes_to_qword(const uint8_t bytes[8]) {
    uint64_t value = 0;
    for (int index = 0; index < 8; ++index) {
        value |= ((uint64_t)bytes[index]) << (index * 8);
    }
    return value;
}

static inline uint64_t apply_palignr64(uint64_t src1, uint64_t src2, uint8_t imm8) {
    uint8_t src1_bytes[8] = {};
    uint8_t src2_bytes[8] = {};
    uint8_t concat_bytes[16] = {};
    uint8_t result_bytes[8] = {};
    palignr_qword_to_bytes(src1, src1_bytes);
    palignr_qword_to_bytes(src2, src2_bytes);
    CPUEAXH_MEMCPY(concat_bytes, src2_bytes, 8);
    CPUEAXH_MEMCPY(concat_bytes + 8, src1_bytes, 8);
    for (int index = 0; index < 8; ++index) {
        const int source_index = (int)imm8 + index;
        result_bytes[index] = (source_index >= 0 && source_index < 16) ? concat_bytes[source_index] : 0x00u;
    }
    return palignr_bytes_to_qword(result_bytes);
}

static inline void validate_palignr_xmm_alignment(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (!inst || ((inst->modrm >> 6) & 0x03) == 0x03) {
        return;
    }
    if ((inst->mem_address & 0x0FULL) != 0) {
        raise_gp_ctx(ctx, 0);
    }
}

static inline DecodedInstruction decode_palignr_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, uint8_t* mandatory_prefix) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_unsupported_simd_prefix = false;
    *mandatory_prefix = 0;

    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;

    while (offset < code_size) {
        const uint8_t prefix = code[offset];
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3) {
            if (*mandatory_prefix == 0 || *mandatory_prefix == prefix) {
                *mandatory_prefix = prefix;
            }
            else {
                has_unsupported_simd_prefix = true;
            }
            if (prefix == 0x66) {
                ctx->operand_size_override = true;
            }
            ++offset;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            ++offset;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            ctx->rex_present = true;
            ctx->rex_w = ((prefix >> 3) & 1) != 0;
            ctx->rex_r = ((prefix >> 2) & 1) != 0;
            ctx->rex_x = ((prefix >> 1) & 1) != 0;
            ctx->rex_b = (prefix & 1) != 0;
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

    if (has_lock_prefix || has_unsupported_simd_prefix || (*mandatory_prefix != 0 && *mandatory_prefix != 0x66)) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (offset + 5 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset++] != 0x0F || code[offset++] != 0x3A || code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_movdq(ctx, &inst, code, code_size, &offset, false);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    inst.opcode = 0x0F;
    inst.mandatory_prefix = *mandatory_prefix;
    inst.imm_size = 1;
    inst.immediate = code[offset++];
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_palignr(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    uint8_t mandatory_prefix = 0;
    const DecodedInstruction inst = decode_palignr_instruction(ctx, code, code_size, &mandatory_prefix);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const uint8_t mod = (inst.modrm >> 6) & 0x03;
    const uint8_t imm8 = (uint8_t)inst.immediate;
    if (mandatory_prefix == 0x66) {
        validate_palignr_xmm_alignment(ctx, &inst);
        if (cpu_has_exception(ctx)) {
            return;
        }

        const int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
        const XMMRegister src1 = get_xmm128(ctx, dest);
        const XMMRegister src2 = (mod == 0x03)
            ? get_xmm128(ctx, decode_movdq_xmm_rm_index(ctx, inst.modrm))
            : read_xmm_memory(ctx, inst.mem_address);
        set_xmm128(ctx, dest, apply_avx2_palignr128(src1, src2, imm8));
        return;
    }

    const int dest = decode_movdq_mm_reg_index(inst.modrm);
    const uint64_t src1 = get_mm64(ctx, dest);
    const uint64_t src2 = (mod == 0x03)
        ? get_mm64(ctx, decode_movdq_mm_rm_index(inst.modrm))
        : read_memory_qword(ctx, inst.mem_address);
    set_mm64(ctx, dest, apply_palignr64(src1, src2, imm8));
}

static inline void palignr_pack_zmm_to_bytes(ZMMRegister value, uint8_t bytes[64]) {
    for (int index = 0; index < 8; ++index) {
        bytes[index] = (uint8_t)((value.xmm0.low >> (index * 8)) & 0xFFu);
        bytes[index + 8] = (uint8_t)((value.xmm0.high >> (index * 8)) & 0xFFu);
        bytes[index + 16] = (uint8_t)((value.xmm1.low >> (index * 8)) & 0xFFu);
        bytes[index + 24] = (uint8_t)((value.xmm1.high >> (index * 8)) & 0xFFu);
        bytes[index + 32] = (uint8_t)((value.xmm2.low >> (index * 8)) & 0xFFu);
        bytes[index + 40] = (uint8_t)((value.xmm2.high >> (index * 8)) & 0xFFu);
        bytes[index + 48] = (uint8_t)((value.xmm3.low >> (index * 8)) & 0xFFu);
        bytes[index + 56] = (uint8_t)((value.xmm3.high >> (index * 8)) & 0xFFu);
    }
}

static inline ZMMRegister palignr_pack_bytes_to_zmm(const uint8_t bytes[64]) {
    ZMMRegister value = {};
    for (int index = 0; index < 8; ++index) {
        value.xmm0.low |= (uint64_t)bytes[index] << (index * 8);
        value.xmm0.high |= (uint64_t)bytes[index + 8] << (index * 8);
        value.xmm1.low |= (uint64_t)bytes[index + 16] << (index * 8);
        value.xmm1.high |= (uint64_t)bytes[index + 24] << (index * 8);
        value.xmm2.low |= (uint64_t)bytes[index + 32] << (index * 8);
        value.xmm2.high |= (uint64_t)bytes[index + 40] << (index * 8);
        value.xmm3.low |= (uint64_t)bytes[index + 48] << (index * 8);
        value.xmm3.high |= (uint64_t)bytes[index + 56] << (index * 8);
    }
    return value;
}

static inline ZMMRegister apply_palignr512(ZMMRegister src1, ZMMRegister src2, int vector_bits, uint8_t imm8) {
    ZMMRegister result = {};
    const int lane_count = vector_bits / 128;
    XMMRegister* result_lanes[4] = { &result.xmm0, &result.xmm1, &result.xmm2, &result.xmm3 };
    XMMRegister src1_lanes[4] = { src1.xmm0, src1.xmm1, src1.xmm2, src1.xmm3 };
    XMMRegister src2_lanes[4] = { src2.xmm0, src2.xmm1, src2.xmm2, src2.xmm3 };
    for (int lane = 0; lane < lane_count; ++lane) {
        *result_lanes[lane] = apply_avx2_palignr128(src1_lanes[lane], src2_lanes[lane], imm8);
    }
    return result;
}

static inline void execute_evex_palignr(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    EVEXDecodedInstructionInfo info = {};
    const DecodedInstruction inst = decode_evex_modrm_imm_instruction(
        ctx,
        code,
        code_size,
        0x0F,
        0x03,
        0x01,
        CPUEAXH_EVEX_VL_128 | CPUEAXH_EVEX_VL_256 | CPUEAXH_EVEX_VL_512,
        true,
        true,
        &info);
    if (cpu_has_exception(ctx)) {
        return;
    }

    const int dest = decode_movdq_xmm_reg_index(ctx, inst.modrm);
    const int src1 = info.source1;
    const int writemask = info.writemask;
    const bool zeroing = info.zeroing;
    const uint8_t mod = (inst.modrm >> 6) & 0x03;
    const uint8_t imm8 = (uint8_t)inst.immediate;
    const int src2_index = decode_movdq_xmm_rm_index(ctx, inst.modrm);
    ZMMRegister dest_old = get_zmm512(ctx, dest);
    ZMMRegister src1_value = get_zmm512(ctx, src1);
    ZMMRegister src2_value = {};
    if (info.vector_bits == 512) {
        src2_value = (mod == 0x03) ? get_zmm512(ctx, src2_index) : read_zmm_memory(ctx, inst.mem_address);
    }
    else if (info.vector_bits == 256) {
        const AVXRegister256 src2_ymm = (mod == 0x03) ? get_ymm256(ctx, src2_index) : read_ymm_memory(ctx, inst.mem_address);
        src2_value.xmm0 = src2_ymm.low;
        src2_value.xmm1 = src2_ymm.high;
    }
    else {
        src2_value.xmm0 = (mod == 0x03) ? get_xmm128(ctx, src2_index) : read_xmm_memory(ctx, inst.mem_address);
    }

    const ZMMRegister computed = apply_palignr512(src1_value, src2_value, info.vector_bits, imm8);
    uint8_t final_bytes[64] = {};
    uint8_t computed_bytes[64] = {};
    uint8_t dest_bytes[64] = {};
    const int active_bytes = info.vector_bits / 8;
    const bool has_writemask = writemask != 0;
    const uint64_t mask_bits = has_writemask ? get_opmask64(ctx, writemask) : ~0ull;

    palignr_pack_zmm_to_bytes(dest_old, dest_bytes);
    palignr_pack_zmm_to_bytes(computed, computed_bytes);
    for (int index = 0; index < active_bytes; ++index) {
        const bool write_lane = !has_writemask || ((mask_bits >> index) & 0x01ull) != 0;
        if (write_lane) {
            final_bytes[index] = computed_bytes[index];
        }
        else {
            final_bytes[index] = zeroing ? 0x00u : dest_bytes[index];
        }
    }
    for (int index = active_bytes; index < 64; ++index) {
        final_bytes[index] = 0x00u;
    }

    set_zmm512(ctx, dest, palignr_pack_bytes_to_zmm(final_bytes));
}
