#pragma once

// instructions/avx_vex_execute.hpp - AVX/VEX instruction dispatch.

#include "avx_vex_ops.hpp"

void execute_avx_vex(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    AVXVexPrefix prefix = decode_avx_vex_prefix(ctx, code, code_size);
    uint8_t opcode = prefix.opcode;
    uint8_t mandatory_prefix = avx_vex_mandatory_prefix(&prefix);
    bool is_256 = avx_vex_is_256(&prefix);
    uint8_t map_select = avx_vex_map_select(&prefix);
    apply_avx_vex_state(ctx, &prefix);

    if (try_execute_bmi_vex(ctx, code, code_size, &prefix)) {
        return;
    }

    if (map_select == 0x03) {
        if ((opcode == 0x14 || opcode == 0x15 || opcode == 0x16) && mandatory_prefix == 1) {
            if (is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            XMMRegister source = get_xmm128(ctx, avx_vex_dest_index(ctx, inst.modrm));
            uint8_t mod = (inst.modrm >> 6) & 0x03;
            uint8_t imm8 = (uint8_t)inst.immediate;
            int dest = decode_movdq_gpr_rm_index(ctx, inst.modrm);

            if (opcode == 0x14) {
                uint8_t source_bytes[16] = {};
                sse2_misc_xmm_to_bytes(source, source_bytes);
                uint8_t value = source_bytes[imm8 & 0x0F];
                if (mod == 0x03) {
                    if (ctx->cs.descriptor.long_mode) {
                        set_reg64(ctx, dest, (uint64_t)value);
                    }
                    else {
                        set_reg32(ctx, dest, (uint32_t)value);
                    }
                }
                else {
                    write_memory_byte(ctx, inst.mem_address, value);
                }
                return;
            }

            if (opcode == 0x15) {
                uint8_t source_bytes[16] = {};
                sse2_misc_xmm_to_bytes(source, source_bytes);
                uint8_t source_offset = (uint8_t)((imm8 & 0x07) * 2);
                uint16_t value = (uint16_t)source_bytes[source_offset]
                               | (uint16_t)(source_bytes[source_offset + 1] << 8);
                if (mod == 0x03) {
                    if (ctx->cs.descriptor.long_mode) {
                        set_reg64(ctx, dest, (uint64_t)value);
                    }
                    else {
                        set_reg32(ctx, dest, (uint32_t)value);
                    }
                }
                else {
                    write_memory_word(ctx, inst.mem_address, value);
                }
                return;
            }

            if (ctx->cs.descriptor.long_mode && ctx->rex_w) {
                uint64_t value = ((imm8 & 0x01) != 0) ? source.high : source.low;
                if (mod == 0x03) {
                    set_reg64(ctx, dest, value);
                }
                else {
                    write_memory_qword(ctx, inst.mem_address, value);
                }
            }
            else {
                uint32_t value = get_xmm_lane_bits(source, imm8 & 0x03);
                if (mod == 0x03) {
                    set_reg32(ctx, dest, value);
                }
                else {
                    write_memory_dword(ctx, inst.mem_address, value);
                }
            }
            return;
        }
        if (opcode == 0x04 || opcode == 0x05) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            uint8_t imm8 = (uint8_t)inst.immediate;
            if (opcode == 0x04) {
                if (is_256) {
                    set_ymm256(ctx, dest, apply_avx_permilps_imm256(read_avx_rm256(ctx, &inst), imm8));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx_permilps_imm128(read_avx_int_rm128(ctx, &inst), imm8));
                    clear_ymm_upper128(ctx, dest);
                }
            }
            else {
                if (is_256) {
                    set_ymm256(ctx, dest, apply_avx_permilpd_imm256(read_avx_rm256(ctx, &inst), imm8));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx_permilpd_imm128(read_avx_int_rm128(ctx, &inst), imm8));
                    clear_ymm_upper128(ctx, dest);
                }
            }
            return;
        }
        if (opcode == 0x00 || opcode == 0x01) {
            if (mandatory_prefix != 1 || !is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            set_ymm256(ctx, dest, apply_avx2_permq256(read_avx_rm256(ctx, &inst), (uint8_t)inst.immediate));
            return;
        }
        if (opcode == 0x0F) {
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            uint8_t imm8 = (uint8_t)inst.immediate;
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx2_palignr256(get_ymm256(ctx, avx_vex_source1_index(&prefix)), read_avx_rm256(ctx, &inst), imm8));
            }
            else {
                set_xmm128(ctx, dest, apply_avx2_palignr128(get_xmm128(ctx, avx_vex_source1_index(&prefix)), read_avx_int_rm128(ctx, &inst), imm8));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x06 || opcode == 0x46) {
            if (!is_256) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            AVXRegister256 src1 = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 src2 = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx_perm2f128_256(src1, src2, (uint8_t)inst.immediate));
            return;
        }
        if (opcode == 0x08 || opcode == 0x09 || opcode == 0x0A || opcode == 0x0B) {
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            uint8_t imm8 = (uint8_t)inst.immediate;
            if (opcode == 0x08) {
                if (avx_vex_requires_reserved_vvvv(&prefix)) {
                    raise_ud_ctx(ctx);
                }
                if (is_256) {
                    set_ymm256(ctx, dest, apply_avx_round_ps256(read_avx_rm256(ctx, &inst), imm8, ctx->mxcsr));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx_round_ps128(read_sse_arith_source_operand(ctx, &inst), imm8, ctx->mxcsr));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            if (opcode == 0x09) {
                if (avx_vex_requires_reserved_vvvv(&prefix)) {
                    raise_ud_ctx(ctx);
                }
                if (is_256) {
                    set_ymm256(ctx, dest, apply_avx_round_pd256(read_avx_rm256(ctx, &inst), imm8, ctx->mxcsr));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx_round_pd128(read_sse2_arith_pd_packed_source(ctx, &inst), imm8, ctx->mxcsr));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            if (opcode == 0x0A) {
                if (is_256) {
                    raise_ud_ctx(ctx);
                }
                XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                set_xmm128(ctx, dest, apply_avx_round_ss128(src1, sse_convert_read_scalar_float_bits(ctx, &inst), imm8, ctx->mxcsr));
                clear_ymm_upper128(ctx, dest);
                return;
            }
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_round_sd128(src1, read_sse2_arith_pd_scalar_source_bits(ctx, &inst), imm8, ctx->mxcsr));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0x02 || opcode == 0x0E || opcode == 0x0C || opcode == 0x0D || opcode == 0x4C) {
            if (opcode == 0x4C && ctx->rex_w) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            uint8_t imm8 = (uint8_t)inst.immediate;
            if (opcode == 0x02) {
                if (is_256) {
                    AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                    AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                    set_ymm256(ctx, dest, apply_avx_blend_ps256(lhs, rhs, imm8));
                }
                else {
                    XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                    XMMRegister rhs = read_avx_int_rm128(ctx, &inst);
                    set_xmm128(ctx, dest, apply_avx_blend_ps128(lhs, rhs, imm8));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            if (opcode == 0x0E) {
                if (is_256) {
                    AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                    AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                    set_ymm256(ctx, dest, apply_avx2_blendw256(lhs, rhs, imm8));
                }
                else {
                    XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                    XMMRegister rhs = read_avx_int_rm128(ctx, &inst);
                    set_xmm128(ctx, dest, apply_avx2_blendw128(lhs, rhs, imm8));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            if (opcode == 0x0C) {
                if (is_256) {
                    AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                    AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                    set_ymm256(ctx, dest, apply_avx_blend_ps256(lhs, rhs, imm8));
                }
                else {
                    XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                    XMMRegister rhs = read_sse_arith_source_operand(ctx, &inst);
                    set_xmm128(ctx, dest, apply_avx_blend_ps128(lhs, rhs, imm8));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            if (opcode == 0x0D) {
                if (is_256) {
                    AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                    AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                    set_ymm256(ctx, dest, apply_avx_blend_pd256(lhs, rhs, imm8));
                }
                else {
                    XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                    XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
                    set_xmm128(ctx, dest, apply_avx_blend_pd128(lhs, rhs, imm8));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            int mask_reg = avx_vex_is4_source_index(ctx, imm8);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                AVXRegister256 mask = get_ymm256(ctx, mask_reg);
                set_ymm256(ctx, dest, apply_avx2_blendvb256(lhs, rhs, mask));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_avx_int_rm128(ctx, &inst);
                XMMRegister mask = get_xmm128(ctx, mask_reg);
                set_xmm128(ctx, dest, apply_avx2_blendvb128(lhs, rhs, mask));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x21) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            uint8_t imm8 = (uint8_t)inst.immediate;
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                XMMRegister src2 = get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm));
                set_xmm128(ctx, dest, apply_avx_insertps_reg128(src1, src2, imm8));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_insertps_mem128(src1, read_memory_dword(ctx, inst.mem_address), imm8));
            }
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0xDF) {
            if (is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            XMMRegister source = read_avx_int_rm128(ctx, &inst);
            set_xmm128(ctx, dest, apply_aeskeygenassist128(source, (uint8_t)inst.immediate));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0x20 || opcode == 0x22) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            uint8_t mod = (inst.modrm >> 6) & 0x03;
            uint8_t imm8 = (uint8_t)inst.immediate;
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            int src2 = decode_movdq_gpr_rm_index(ctx, inst.modrm);
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));

            if (opcode == 0x20) {
                uint8_t value = mod == 0x03
                    ? (uint8_t)get_reg64(ctx, src2)
                    : read_memory_byte(ctx, inst.mem_address);
                set_xmm128(ctx, dest, apply_avx_pinsrb128(src1, value, imm8));
                clear_ymm_upper128(ctx, dest);
                return;
            }

            if (ctx->rex_w) {
                if (!ctx->cs.descriptor.long_mode) {
                    raise_ud_ctx(ctx);
                }
                uint64_t value = mod == 0x03
                    ? get_reg64(ctx, src2)
                    : read_memory_qword(ctx, inst.mem_address);
                set_xmm128(ctx, dest, apply_avx_pinsrq128(src1, value, imm8));
            }
            else {
                uint32_t value = mod == 0x03
                    ? (uint32_t)get_reg64(ctx, src2)
                    : read_memory_dword(ctx, inst.mem_address);
                set_xmm128(ctx, dest, apply_avx_pinsrd128(src1, value, imm8));
            }
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0x17) {
            if (ctx->rex_w || is_256 || avx_vex_encoded_vvvv(&prefix) != 0x00) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            uint32_t value = get_xmm_lane_bits(get_xmm128(ctx, avx_vex_dest_index(ctx, inst.modrm)),
                                               (int)((uint8_t)inst.immediate & 0x03));
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                set_reg32(ctx, avx_vex_rm_index(ctx, inst.modrm), value);
            }
            else {
                write_memory_dword(ctx, inst.mem_address, value);
            }
            return;
        }
        if (opcode == 0x42) {
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            uint8_t imm8 = (uint8_t)inst.immediate;
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_mpsadbw256(get_ymm256(ctx, avx_vex_source1_index(&prefix)),
                                                           read_avx_rm256(ctx, &inst),
                                                           imm8));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_mpsadbw128(get_xmm128(ctx, avx_vex_source1_index(&prefix)),
                                                           read_avx_int_rm128(ctx, &inst),
                                                           imm8));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x60 || opcode == 0x61 || opcode == 0x62 || opcode == 0x63) {
            if (mandatory_prefix != 1 || is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            uint8_t imm8 = (uint8_t)inst.immediate;
            __m128i lhs = avx_xmm_to_m128i(get_xmm128(ctx, avx_vex_dest_index(ctx, inst.modrm)));
            __m128i rhs = avx_xmm_to_m128i(read_avx_int_rm128(ctx, &inst));

            if (opcode == 0x60 || opcode == 0x61) {
                int length_a = avx_pcmpestr_length(ctx, REG_RAX, ctx->rex_w, imm8);
                int length_b = avx_pcmpestr_length(ctx, REG_RDX, ctx->rex_w, imm8);
                AVXPcmpstrResult result = avx_execute_pcmpestr(lhs, length_a, rhs, length_b, imm8);
                update_avx_pcmpstr_flags(ctx, result.cf, result.zf, result.sf, result.of);
                if (opcode == 0x60) {
                    set_xmm128(ctx, 0, result.mask);
                    clear_ymm_upper128(ctx, 0);
                }
                else {
                    set_reg32(ctx, REG_RCX, result.index);
                }
                return;
            }

            AVXPcmpstrResult result = avx_execute_pcmpistr(lhs, rhs, imm8);
            update_avx_pcmpstr_flags(ctx, result.cf, result.zf, result.sf, result.of);
            if (opcode == 0x62) {
                set_xmm128(ctx, 0, result.mask);
                clear_ymm_upper128(ctx, 0);
            }
            else {
                set_reg32(ctx, REG_RCX, result.index);
            }
            return;
        }
        if (opcode == 0x18 || opcode == 0x38) {
            if (mandatory_prefix != 1 || !is_256) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            AVXRegister256 src1 = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            XMMRegister insert_value = ((inst.modrm >> 6) & 0x03) == 0x03
                ? get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm))
                : read_xmm_memory(ctx, inst.mem_address);
            set_ymm256(ctx, dest, apply_avx_insertf128_256(src1, insert_value, (uint8_t)inst.immediate));
            return;
        }
        if (opcode == 0x19 || opcode == 0x39) {
            if (mandatory_prefix != 1 || !is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            XMMRegister extracted = apply_avx_extractf128_256(get_ymm256(ctx, avx_vex_dest_index(ctx, inst.modrm)), (uint8_t)inst.immediate);
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                int dest = avx_vex_rm_index(ctx, inst.modrm);
                set_xmm128(ctx, dest, extracted);
                clear_ymm_upper128(ctx, dest);
            }
            else {
                write_xmm_memory(ctx, inst.mem_address, extracted);
            }
            return;
        }
        if (opcode == 0x4A || opcode == 0x4B) {
            if (ctx->rex_w) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            int mask_reg = avx_vex_is4_source_index(ctx, (uint8_t)inst.immediate);
            if (opcode == 0x4A) {
                if (is_256) {
                    AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                    AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                    AVXRegister256 mask = get_ymm256(ctx, mask_reg);
                    set_ymm256(ctx, dest, apply_avx_blendv_ps256(lhs, rhs, mask));
                }
                else {
                    XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                    XMMRegister rhs = read_sse_arith_source_operand(ctx, &inst);
                    XMMRegister mask = get_xmm128(ctx, mask_reg);
                    set_xmm128(ctx, dest, apply_avx_blendv_ps128(lhs, rhs, mask));
                    clear_ymm_upper128(ctx, dest);
                }
                return;
            }
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                AVXRegister256 mask = get_ymm256(ctx, mask_reg);
                set_ymm256(ctx, dest, apply_avx_blendv_pd256(lhs, rhs, mask));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
                XMMRegister mask = get_xmm128(ctx, mask_reg);
                set_xmm128(ctx, dest, apply_avx_blendv_pd128(lhs, rhs, mask));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x40 || opcode == 0x41) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_0f3a_modrm_imm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (opcode == 0x40) {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_arith_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_dpps128(lhs, rhs, (uint8_t)inst.immediate));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_dppd128(lhs, rhs, (uint8_t)inst.immediate));
            }
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (map_select == 0x02) {
        if (opcode == 0xDB) {
            if (mandatory_prefix != 1 || is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            XMMRegister source = read_avx_int_rm128(ctx, &inst);
            set_xmm128(ctx, dest, apply_aesimc128(source));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0x41) {
            if (mandatory_prefix != 1 || is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            set_xmm128(ctx, dest, apply_avx_phminposuw128(read_avx_int_rm128(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0xDC) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 state = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 round_key = read_avx_rm256(ctx, &inst);
                AVXRegister256 result = {};
                result.low = apply_aesenc128(state.low, round_key.low);
                result.high = apply_aesenc128(state.high, round_key.high);
                set_ymm256(ctx, dest, result);
            }
            else {
                XMMRegister state = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister round_key = read_avx_int_rm128(ctx, &inst);
                set_xmm128(ctx, dest, apply_aesenc128(state, round_key));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0xDD) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }

            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 state = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 round_key = read_avx_rm256(ctx, &inst);
                AVXRegister256 result = {};
                result.low = apply_aesenclast128(state.low, round_key.low);
                result.high = apply_aesenclast128(state.high, round_key.high);
                set_ymm256(ctx, dest, result);
            }
            else {
                XMMRegister state = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister round_key = read_avx_int_rm128(ctx, &inst);
                set_xmm128(ctx, dest, apply_aesenclast128(state, round_key));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x00) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx2_pshufb256(get_ymm256(ctx, avx_vex_source1_index(&prefix)), read_avx_rm256(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx2_pshufb128(get_xmm128(ctx, avx_vex_source1_index(&prefix)), read_avx_int_rm128(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x16 || opcode == 0x36) {
            if (mandatory_prefix != 1 || !is_256) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            AVXRegister256 control = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 data = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx2_perm32x8(data, control));
            return;
        }
        if (opcode == 0x45 || opcode == 0x46 || opcode == 0x47) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            bool is_qword = ctx->rex_w && opcode != 0x46;
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 src = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 counts = read_avx_rm256(ctx, &inst);
                if (is_qword) {
                    set_ymm256(ctx, dest, apply_avx2_shiftvar_qword256(ctx, opcode, src, counts));
                }
                else {
                    set_ymm256(ctx, dest, apply_avx2_shiftvar_dword256(ctx, opcode, src, counts));
                }
            }
            else {
                XMMRegister src = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister counts = read_avx_int_rm128(ctx, &inst);
                if (is_qword) {
                    set_xmm128(ctx, dest, apply_avx2_shiftvar_qword128(ctx, opcode, src, counts));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx2_shiftvar_dword128(ctx, opcode, src, counts));
                }
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x2A) {
            if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                raise_ud_ctx(ctx);
            }
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            validate_avx_movaps_alignment(ctx, &inst, is_256);
            if (is_256) {
                set_ymm256(ctx, dest, read_ymm_memory(ctx, inst.mem_address));
            }
            else {
                set_xmm128(ctx, dest, read_xmm_memory(ctx, inst.mem_address));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode >= 0x90 && opcode <= 0x93) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            if (opcode == 0x90 || opcode == 0x92) {
                if (ctx->rex_w) {
                    execute_avx_gather_dpd(ctx, &prefix, &inst, is_256);
                }
                else {
                    execute_avx_gather_dps(ctx, &prefix, &inst, is_256);
                }
            }
            else {
                if (ctx->rex_w) {
                    execute_avx_gather_qpd(ctx, &prefix, &inst, is_256);
                }
                else {
                    execute_avx_gather_qps(ctx, &prefix, &inst, is_256);
                }
            }
            return;
        }
        if (opcode == 0x99 || opcode == 0xA9 || opcode == 0xB9) {
            if (mandatory_prefix != 1 || !ctx->rex_w) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            XMMRegister src1 = get_xmm128(ctx, dest);
            XMMRegister src2 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            uint64_t src3 = read_sse2_arith_pd_scalar_source_bits(ctx, &inst);
            set_xmm128(ctx, dest, apply_avx_fmadd_sd128(ctx, opcode, src1, src2, src3));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (opcode == 0x8C || opcode == 0x8E) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                raise_ud_ctx(ctx);
            }

            int reg = avx_vex_dest_index(ctx, inst.modrm);
            int mask_reg = avx_vex_source1_index(&prefix);
            bool is_store = opcode == 0x8E;
            bool is_qword = ctx->rex_w;

            if (!is_store) {
                if (is_256) {
                    AVXRegister256 mask = get_ymm256(ctx, mask_reg);
                    AVXRegister256 memory_value = read_ymm_memory(ctx, inst.mem_address);
                    set_ymm256(ctx, reg, apply_avx2_pmaskmov_load256(mask, memory_value, is_qword));
                }
                else {
                    XMMRegister mask = get_xmm128(ctx, mask_reg);
                    XMMRegister memory_value = read_xmm_memory(ctx, inst.mem_address);
                    set_xmm128(ctx, reg, apply_avx2_pmaskmov_load128(mask, memory_value, is_qword));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    execute_avx2_pmaskmov_store256(ctx, inst.mem_address, get_ymm256(ctx, mask_reg), get_ymm256(ctx, reg), is_qword);
                }
                else {
                    execute_avx2_pmaskmov_store128(ctx, inst.mem_address, get_xmm128(ctx, mask_reg), get_xmm128(ctx, reg), is_qword);
                }
            }
            return;
        }
        if ((opcode >= 0x20 && opcode <= 0x25) || (opcode >= 0x30 && opcode <= 0x35)) {
            if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            XMMRegister source = read_avx2_pmov_source(ctx, &inst, opcode, is_256);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx2_pmovx256(ctx, opcode, source));
            }
            else {
                set_xmm128(ctx, dest, apply_avx2_pmovx128(ctx, opcode, source));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x58 || opcode == 0x59 || opcode == 0x78 || opcode == 0x79) {
            if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            uint8_t mod = (inst.modrm >> 6) & 0x03;
            XMMRegister src = mod == 0x03 ? get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm)) : XMMRegister{};
            AVXRegister256 result = {};
            if (opcode == 0x58) {
                uint32_t value = mod == 0x03 ? get_xmm_lane_bits(src, 0) : read_memory_dword(ctx, inst.mem_address);
                result = broadcast_avx2_dword256(value);
            }
            else if (opcode == 0x59) {
                uint64_t value = mod == 0x03 ? src.low : read_memory_qword(ctx, inst.mem_address);
                result = broadcast_avx2_qword256(value);
            }
            else if (opcode == 0x78) {
                uint8_t value = mod == 0x03 ? (uint8_t)(src.low & 0xFFU) : read_memory_byte(ctx, inst.mem_address);
                result = broadcast_avx2_byte256(value);
            }
            else {
                uint16_t value = mod == 0x03 ? (uint16_t)(src.low & 0xFFFFU) : read_memory_word(ctx, inst.mem_address);
                result = broadcast_avx2_word256(value);
            }
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                set_ymm256(ctx, dest, result);
            }
            else {
                set_xmm128(ctx, dest, result.low);
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x18 || opcode == 0x19 || opcode == 0x1A || opcode == 0x5A) {
            if (mandatory_prefix != 1 || !is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                raise_ud_ctx(ctx);
            }
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (opcode == 0x18) {
                set_ymm256(ctx, dest, broadcast_avx_ss256(read_memory_dword(ctx, inst.mem_address)));
            }
            else if (opcode == 0x19) {
                set_ymm256(ctx, dest, broadcast_avx_sd256(read_memory_qword(ctx, inst.mem_address)));
            }
            else {
                set_ymm256(ctx, dest, broadcast_avx_f128_256(read_xmm_memory(ctx, inst.mem_address)));
            }
            return;
        }
        if (opcode == 0x0C || opcode == 0x0D) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (opcode == 0x0C) {
                if (is_256) {
                    set_ymm256(ctx, dest, apply_avx_permilps_var256(get_ymm256(ctx, avx_vex_source1_index(&prefix)), read_avx_rm256(ctx, &inst)));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx_permilps_var128(get_xmm128(ctx, avx_vex_source1_index(&prefix)), read_avx_int_rm128(ctx, &inst)));
                    clear_ymm_upper128(ctx, dest);
                }
            }
            else {
                if (is_256) {
                    set_ymm256(ctx, dest, apply_avx_permilpd_var256(get_ymm256(ctx, avx_vex_source1_index(&prefix)), read_avx_rm256(ctx, &inst)));
                }
                else {
                    set_xmm128(ctx, dest, apply_avx_permilpd_var128(get_xmm128(ctx, avx_vex_source1_index(&prefix)), read_avx_int_rm128(ctx, &inst)));
                    clear_ymm_upper128(ctx, dest);
                }
            }
            return;
        }
        if (opcode == 0x2C || opcode == 0x2D || opcode == 0x2E || opcode == 0x2F) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            if (((inst.modrm >> 6) & 0x03) == 0x03) {
                raise_ud_ctx(ctx);
            }
            int reg = avx_vex_dest_index(ctx, inst.modrm);
            int mask_reg = avx_vex_source1_index(&prefix);
            bool is_store = opcode == 0x2E || opcode == 0x2F;
            bool is_pd = opcode == 0x2D || opcode == 0x2F;
            if (!is_store) {
                if (is_256) {
                    AVXRegister256 memory_value = read_ymm_memory(ctx, inst.mem_address);
                    AVXRegister256 mask = get_ymm256(ctx, mask_reg);
                    set_ymm256(ctx, reg, is_pd ? apply_avx_maskmov_load_pd256(mask, memory_value)
                                                : apply_avx_maskmov_load_ps256(mask, memory_value));
                }
                else {
                    XMMRegister memory_value = read_xmm_memory(ctx, inst.mem_address);
                    XMMRegister mask = get_xmm128(ctx, mask_reg);
                    set_xmm128(ctx, reg, is_pd ? apply_avx_maskmov_load_pd128(mask, memory_value)
                                               : apply_avx_maskmov_load_ps128(mask, memory_value));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    AVXRegister256 mask = get_ymm256(ctx, mask_reg);
                    AVXRegister256 source = get_ymm256(ctx, reg);
                    if (is_pd) {
                        execute_avx_maskmov_store_pd256(ctx, inst.mem_address, mask, source);
                    }
                    else {
                        execute_avx_maskmov_store_ps256(ctx, inst.mem_address, mask, source);
                    }
                }
                else {
                    XMMRegister mask = get_xmm128(ctx, mask_reg);
                    XMMRegister source = get_xmm128(ctx, reg);
                    if (is_pd) {
                        execute_avx_maskmov_store_pd128(ctx, inst.mem_address, mask, source);
                    }
                    else {
                        execute_avx_maskmov_store_ps128(ctx, inst.mem_address, mask, source);
                    }
                }
            }
            return;
        }
        if (opcode == 0x0E || opcode == 0x0F || opcode == 0x17) {
            if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int src1 = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, src1);
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_avx_test_flags(ctx, compute_avx_test_zf256(lhs, rhs), compute_avx_test_cf256(lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, src1);
                XMMRegister rhs = read_avx_int_rm128(ctx, &inst);
                set_avx_test_flags(ctx, compute_avx_test_zf128(lhs, rhs), compute_avx_test_cf128(lhs, rhs));
            }
            return;
        }
        if (opcode == 0x04 || opcode == 0x0B || opcode == 0x28 || opcode == 0x40) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_int_arith256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_int_arith_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_int_arith128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x01 || opcode == 0x02 || opcode == 0x03 || opcode == 0x05 || opcode == 0x06 || opcode == 0x07) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx2_horizontal256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_pack_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx2_horizontal128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x29 || opcode == 0x37) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_int_cmp256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_int_cmp_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_int_cmp128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x2B) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_pack256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_pack_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_pack128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x38 || opcode == 0x39 || opcode == 0x3A || opcode == 0x3B || opcode == 0x3C || opcode == 0x3D || opcode == 0x3E || opcode == 0x3F) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_int_minmax256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_pack_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_int_minmax128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x08 || opcode == 0x09 || opcode == 0x0A) {
            if (mandatory_prefix != 1) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx2_sign256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_pack_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx2_sign128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (opcode == 0x1C || opcode == 0x1D || opcode == 0x1E) {
            if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx2_abs256(ctx, opcode, read_avx_rm256(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx2_abs128(ctx, opcode, read_sse2_pack_source_operand(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (map_select != 0x01) {
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x77) {
        if (mandatory_prefix != 0 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        if (is_256) {
            clear_all_avx_registers(ctx);
        }
        else {
            clear_all_ymm_upper128(ctx);
        }
        ctx->last_inst_size = (int)prefix.modrm_offset;
        return;
    }

    if (opcode == 0x10 || opcode == 0x11) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int reg = avx_vex_dest_index(ctx, inst.modrm);
        uint8_t mod = (inst.modrm >> 6) & 0x03;
        if (mandatory_prefix == 0) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x10) {
                if (is_256) {
                    set_ymm256(ctx, reg, read_avx_rm256(ctx, &inst));
                }
                else {
                    set_xmm128(ctx, reg, read_movups_rm128(ctx, &inst));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    write_avx_rm256(ctx, &inst, get_ymm256(ctx, reg));
                }
                else {
                    write_movups_rm128(ctx, &inst, get_xmm128(ctx, reg));
                }
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x10) {
                if (is_256) {
                    set_ymm256(ctx, reg, read_avx_rm256(ctx, &inst));
                }
                else {
                    set_xmm128(ctx, reg, read_sse2_mov_pd_rm128(ctx, &inst));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    write_avx_rm256(ctx, &inst, get_ymm256(ctx, reg));
                }
                else {
                    write_sse2_mov_pd_rm128(ctx, &inst, get_xmm128(ctx, reg));
                }
            }
            return;
        }
        if (mandatory_prefix == 2) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x10) {
                XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                set_xmm128(ctx, reg, apply_avx_movss_load128(src1, sse_convert_read_scalar_float_bits(ctx, &inst)));
                clear_ymm_upper128(ctx, reg);
            }
            else {
                if (avx_vex_requires_reserved_vvvv(&prefix)) {
                    raise_ud_ctx(ctx);
                }
                XMMRegister src = get_xmm128(ctx, reg);
                if (mod == 3) {
                    int dest = avx_vex_rm_index(ctx, inst.modrm);
                    XMMRegister result = get_xmm128(ctx, dest);
                    set_xmm_lane_bits(&result, 0, get_xmm_lane_bits(src, 0));
                    set_xmm128(ctx, dest, result);
                }
                else {
                    write_memory_dword(ctx, inst.mem_address, get_xmm_lane_bits(src, 0));
                }
            }
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x10) {
                XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister result = {};
                result.low = (mod == 3)
                    ? get_xmm128(ctx, decode_sse2_mov_pd_xmm_rm_index(ctx, inst.modrm)).low
                    : read_memory_qword(ctx, inst.mem_address);
                result.high = src1.high;
                set_xmm128(ctx, reg, result);
                clear_ymm_upper128(ctx, reg);
            }
            else {
                if (avx_vex_requires_reserved_vvvv(&prefix)) {
                    raise_ud_ctx(ctx);
                }
                XMMRegister src = get_xmm128(ctx, reg);
                if (mod == 3) {
                    int dest = decode_sse2_mov_pd_xmm_rm_index(ctx, inst.modrm);
                    XMMRegister result = get_xmm128(ctx, dest);
                    result.low = src.low;
                    set_xmm128(ctx, dest, result);
                }
                else {
                    write_memory_qword(ctx, inst.mem_address, src.low);
                }
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x12 || opcode == 0x13 || opcode == 0x16 || opcode == 0x17) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        uint8_t mod = (inst.modrm >> 6) & 0x03;
        int reg = avx_vex_dest_index(ctx, inst.modrm);

        if (map_select == 0x01 && (mandatory_prefix == 2 || mandatory_prefix == 3)) {
            if (opcode == 0x13 || opcode == 0x17 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }

            if (mandatory_prefix == 2) {
                if (opcode != 0x12) {
                    raise_ud_ctx(ctx);
                }
                if (is_256) {
                    set_ymm256(ctx, reg, apply_avx_movddup256(read_avx_rm256(ctx, &inst)));
                }
                else {
                    XMMRegister result = mod == 0x03
                        ? apply_avx_movddup128(get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm)))
                        : apply_avx_movddup_load128(read_memory_qword(ctx, inst.mem_address));
                    set_xmm128(ctx, reg, result);
                    clear_ymm_upper128(ctx, reg);
                }
                return;
            }

            if (opcode == 0x12) {
                if (is_256) {
                    set_ymm256(ctx, reg, apply_avx_movsldup256(read_avx_rm256(ctx, &inst)));
                }
                else {
                    set_xmm128(ctx, reg, apply_avx_movsldup128(read_avx_int_rm128(ctx, &inst)));
                    clear_ymm_upper128(ctx, reg);
                }
                return;
            }

            if (opcode == 0x16) {
                if (is_256) {
                    set_ymm256(ctx, reg, apply_avx_movshdup256(read_avx_rm256(ctx, &inst)));
                }
                else {
                    set_xmm128(ctx, reg, apply_avx_movshdup128(read_avx_int_rm128(ctx, &inst)));
                    clear_ymm_upper128(ctx, reg);
                }
                return;
            }

            raise_ud_ctx(ctx);
        }

        if (mandatory_prefix == 0) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x12) {
                XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister result = mod == 3
                    ? apply_avx_movhlps128(src1, get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm)))
                    : apply_avx_movlps_load128(src1, read_memory_qword(ctx, inst.mem_address));
                set_xmm128(ctx, reg, result);
                clear_ymm_upper128(ctx, reg);
                return;
            }
            if (opcode == 0x16) {
                XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister result = mod == 3
                    ? apply_avx_movlhps128(src1, get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm)))
                    : apply_avx_movhps_load128(src1, read_memory_qword(ctx, inst.mem_address));
                set_xmm128(ctx, reg, result);
                clear_ymm_upper128(ctx, reg);
                return;
            }
            if (mod == 3 || avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src = get_xmm128(ctx, reg);
            write_memory_qword(ctx, inst.mem_address, opcode == 0x13 ? src.low : src.high);
            return;
        }

        if (mandatory_prefix != 1 || is_256) {
            raise_ud_ctx(ctx);
        }
        if (mod == 3) {
            raise_ud_ctx(ctx);
        }
        if (opcode == 0x12) {
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, reg, apply_avx_movlps_load128(src1, read_memory_qword(ctx, inst.mem_address)));
            clear_ymm_upper128(ctx, reg);
            return;
        }
        if (opcode == 0x16) {
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, reg, apply_avx_movhps_load128(src1, read_memory_qword(ctx, inst.mem_address)));
            clear_ymm_upper128(ctx, reg);
            return;
        }
        if (avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        XMMRegister src = get_xmm128(ctx, reg);
        write_memory_qword(ctx, inst.mem_address, opcode == 0x13 ? src.low : src.high);
        return;
    }

    if ((opcode == 0x28 || opcode == 0x29) && !(opcode == 0x28 && map_select == 0x02 && mandatory_prefix == 1)) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int reg = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x28) {
                if (is_256) {
                    set_ymm256(ctx, reg, read_avx_movaps_rm256(ctx, &inst));
                }
                else {
                    set_xmm128(ctx, reg, read_movaps_rm128(ctx, &inst));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    write_avx_movaps_rm256(ctx, &inst, get_ymm256(ctx, reg));
                }
                else {
                    write_movaps_rm128(ctx, &inst, get_xmm128(ctx, reg));
                }
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (opcode == 0x28) {
                if (is_256) {
                    set_ymm256(ctx, reg, read_avx_movaps_rm256(ctx, &inst));
                }
                else {
                    validate_avx_movaps_alignment(ctx, &inst, false);
                    set_xmm128(ctx, reg, read_sse2_mov_pd_rm128(ctx, &inst));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    write_avx_movaps_rm256(ctx, &inst, get_ymm256(ctx, reg));
                }
                else {
                    validate_avx_movaps_alignment(ctx, &inst, false);
                    write_sse2_mov_pd_rm128(ctx, &inst, get_xmm128(ctx, reg));
                }
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x7E && map_select == 0x01 && mandatory_prefix == 2 && !is_256) {
        if (avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }

        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        XMMRegister result = {};
        result.low = read_movdq_xmm_low_or_mem64(ctx, &inst);
        result.high = 0;
        set_xmm128(ctx, dest, result);
        clear_ymm_upper128(ctx, dest);
        return;
    }

    if (opcode == 0x6E || opcode == 0x7E) {
        if (map_select != 0x01 || mandatory_prefix != 1 || is_256) {
            raise_ud_ctx(ctx);
        }

        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int operand_size = (ctx->cs.descriptor.long_mode && ctx->rex_w) ? 64 : 32;

        if (opcode == 0x6E) {
            XMMRegister result = {};
            int dest = avx_vex_dest_index(ctx, inst.modrm);
            result.low = read_movdq_gpr_or_mem(ctx, &inst, operand_size);
            result.high = 0;
            set_xmm128(ctx, dest, result);
            clear_ymm_upper128(ctx, dest);
            return;
        }

        XMMRegister src = get_xmm128(ctx, avx_vex_dest_index(ctx, inst.modrm));
        write_movdq_gpr_or_mem(ctx, &inst, operand_size, src.low);
        return;
    }

    if (opcode == 0xAE) {
        if (map_select != 0x01 || mandatory_prefix != 0 || is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }

        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        uint8_t reg = (inst.modrm >> 3) & 0x07;
        if (((inst.modrm >> 6) & 0x03) == 0x03) {
            raise_ud_ctx(ctx);
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

        raise_ud_ctx(ctx);
    }

    if (opcode == 0xD0) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_addsub_pd256(lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_math_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_addsub_pd128(lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_addsub_ps256(lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_math_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_addsub_ps128(lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x2B) {
        if (avx_vex_requires_reserved_vvvv(&prefix) || (mandatory_prefix != 0 && mandatory_prefix != 1)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        if (((inst.modrm >> 6) & 0x03) == 0x03) {
            raise_ud_ctx(ctx);
        }
        int src = avx_vex_dest_index(ctx, inst.modrm);
        validate_avx_movaps_alignment(ctx, &inst, is_256);
        if (is_256) {
            write_ymm_memory(ctx, inst.mem_address, get_ymm256(ctx, src));
        }
        else {
            write_xmm_memory(ctx, inst.mem_address, get_xmm128(ctx, src));
        }
        return;
    }
    if (opcode == 0x6F || opcode == 0x7F) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int reg = avx_vex_dest_index(ctx, inst.modrm);
        if (avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        if (mandatory_prefix == 1) {
            if (opcode == 0x6F) {
                if (is_256) {
                    set_ymm256(ctx, reg, read_avx_movaps_rm256(ctx, &inst));
                }
                else {
                    set_xmm128(ctx, reg, read_avx_movdqa_rm128(ctx, &inst));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    write_avx_movaps_rm256(ctx, &inst, get_ymm256(ctx, reg));
                }
                else {
                    write_avx_movdqa_rm128(ctx, &inst, get_xmm128(ctx, reg), true);
                }
            }
            return;
        }
        if (mandatory_prefix == 2) {
            if (opcode == 0x6F) {
                if (is_256) {
                    set_ymm256(ctx, reg, read_avx_rm256(ctx, &inst));
                }
                else {
                    set_xmm128(ctx, reg, read_avx_int_rm128(ctx, &inst));
                    clear_ymm_upper128(ctx, reg);
                }
            }
            else {
                if (is_256) {
                    write_avx_rm256(ctx, &inst, get_ymm256(ctx, reg));
                }
                else {
                    write_avx_int_rm128(ctx, &inst, get_xmm128(ctx, reg), true);
                }
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0xE7) {
        if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        if (((inst.modrm >> 6) & 0x03) == 0x03) {
            raise_ud_ctx(ctx);
        }
        int src = avx_vex_dest_index(ctx, inst.modrm);
        validate_avx_movaps_alignment(ctx, &inst, is_256);
        if (is_256) {
            write_ymm_memory(ctx, inst.mem_address, get_ymm256(ctx, src));
        }
        else {
            write_xmm_memory(ctx, inst.mem_address, get_xmm128(ctx, src));
        }
        return;
    }

    if (opcode == 0x50) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        if (((inst.modrm >> 6) & 0x03) != 0x03) {
            raise_ud_ctx(ctx);
        }
        int dest = decode_sse2_mov_pd_gpr_reg_index(ctx, inst.modrm);
        int src = avx_vex_rm_index(ctx, inst.modrm);
        uint32_t mask = 0;
        if (mandatory_prefix == 0) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            mask = is_256 ? compute_avx_movmskps256(get_ymm256(ctx, src))
                          : compute_avx_movmskps128(get_xmm128(ctx, src));
        }
        else if (mandatory_prefix == 1) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            mask = is_256 ? compute_avx_movmskpd256(get_ymm256(ctx, src))
                          : compute_avx_movmskpd128(get_xmm128(ctx, src));
        }
        else {
            raise_ud_ctx(ctx);
        }
        set_reg32(ctx, dest, mask);
        return;
    }

    if (opcode == 0xD7) {
        if (mandatory_prefix != 1 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        if (((inst.modrm >> 6) & 0x03) != 0x03) {
            raise_ud_ctx(ctx);
        }
        int dest = decode_sse2_mov_pd_gpr_reg_index(ctx, inst.modrm);
        int src = avx_vex_rm_index(ctx, inst.modrm);
        set_reg32(ctx, dest, is_256 ? compute_avx_pmovmskb256(get_ymm256(ctx, src))
                                    : compute_avx_pmovmskb128(get_xmm128(ctx, src)));
        return;
    }

    if (opcode == 0xF0) {
        if (mandatory_prefix != 3 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (is_256) {
            set_ymm256(ctx, dest, read_ymm_memory(ctx, inst.mem_address));
        }
        else {
            set_xmm128(ctx, dest, read_xmm_memory(ctx, inst.mem_address));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0xF7) {
        if (mandatory_prefix != 1 || is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        if (((inst.modrm >> 6) & 0x03) != 0x03) {
            raise_ud_ctx(ctx);
        }
        int src = avx_vex_dest_index(ctx, inst.modrm);
        int mask = avx_vex_rm_index(ctx, inst.modrm);
        uint8_t src_bytes[16] = {};
        uint8_t mask_bytes[16] = {};
        uint64_t address = get_sse2_misc_maskmovdqu_address(ctx, &inst);
        sse2_misc_xmm_to_bytes(get_xmm128(ctx, src), src_bytes);
        sse2_misc_xmm_to_bytes(get_xmm128(ctx, mask), mask_bytes);
        sse2_misc_write_maskmovdqu_bytes(ctx, address, src_bytes, mask_bytes);
        return;
    }

    if (opcode == 0x14 || opcode == 0x15) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_unpack_ps256(opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_shuffle_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_unpack_ps128(opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_unpack_pd256(opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_unpack_pd128(opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0xC6) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        uint8_t imm8 = (uint8_t)inst.immediate;
        if (mandatory_prefix == 0) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_shufps256(lhs, rhs, imm8));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_shuffle_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_shufps128(lhs, rhs, imm8));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_shufpd256(lhs, rhs, imm8));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_shufpd128(lhs, rhs, imm8));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0xC4) {
        if (map_select != 0x01 || mandatory_prefix != 1 || is_256) {
            raise_ud_ctx(ctx);
        }

        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        uint8_t mod = (inst.modrm >> 6) & 0x03;
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        int src2 = decode_movdq_gpr_rm_index(ctx, inst.modrm);
        uint16_t value = mod == 0x03
            ? (uint16_t)get_reg64(ctx, src2)
            : read_memory_word(ctx, inst.mem_address);

        set_xmm128(ctx, dest, apply_avx_pinsrw128(get_xmm128(ctx, avx_vex_source1_index(&prefix)),
                                                  value,
                                                  (uint8_t)inst.immediate));
        clear_ymm_upper128(ctx, dest);
        return;
    }

    if (opcode == 0x54 || opcode == 0x55 || opcode == 0x56 || opcode == 0x57) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_logic256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = (opcode == 0x57) ? read_xorps_source_operand(ctx, &inst) : read_sse_logic_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_logic128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_logic256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_logic_pd_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_logic128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0xDB || opcode == 0xDF || opcode == 0xEB || opcode == 0xEF) {
        if (mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (is_256) {
            AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx_int_logic256(ctx, opcode, lhs, rhs));
        }
        else {
            XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            XMMRegister rhs = read_sse2_int_logic_source_operand(ctx, &inst);
            set_xmm128(ctx, dest, apply_avx_int_logic128(ctx, opcode, lhs, rhs));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0x5A) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_cvtps2pd256(sse2_convert_read_xmm_source(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_cvtps2pd128(sse2_convert_read_low64_source(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (is_256) {
                set_xmm128(ctx, dest, apply_avx_cvtpd2ps256(read_avx_rm256(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_cvtpd2ps128(sse2_convert_read_xmm_source(ctx, &inst)));
            }
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 2) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_cvtss2sd128(src1, sse_convert_read_scalar_float_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_cvtsd2ss128(src1, sse2_scalar_convert_read_scalar_double_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x5B) {
        if (avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_cvtdq2ps256(read_avx_rm256(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_cvtdq2ps128(sse2_convert_read_xmm_source(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1 || mandatory_prefix == 2) {
            bool truncate = mandatory_prefix == 2;
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_cvtps2dq256(read_avx_rm256(ctx, &inst), truncate, ctx->mxcsr));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_cvtps2dq128(sse2_convert_read_xmm_source(ctx, &inst), truncate, ctx->mxcsr));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0xE6) {
        if (avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 2) {
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_cvtdq2pd256(sse2_convert_read_xmm_source(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_cvtdq2pd128(sse2_convert_read_low64_source(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1 || mandatory_prefix == 3) {
            bool truncate = mandatory_prefix == 1;
            if (is_256) {
                set_xmm128(ctx, dest, apply_avx_cvtpd2dq256(read_avx_rm256(ctx, &inst), truncate, ctx->mxcsr));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_cvtpd2dq128(sse2_convert_read_xmm_source(ctx, &inst), truncate, ctx->mxcsr));
            }
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x2A) {
        if (is_256) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        int source_bits = ctx->rex_w ? 64 : 32;
        if (mandatory_prefix == 2) {
            int64_t source_value = sse_convert_read_signed_integer_source(ctx, &inst, source_bits);
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_cvtsi2ss128(src1, source_value));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 3) {
            int64_t source_value = sse2_scalar_convert_read_signed_integer_source(ctx, &inst, source_bits);
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_cvtsi2sd128(src1, source_value));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x2C || opcode == 0x2D) {
        if (is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest_bits = ctx->rex_w ? 64 : 32;
        bool truncate = opcode == 0x2C;
        if (mandatory_prefix == 2) {
            int dest = decode_sse_convert_gpr_reg_index(ctx, inst.modrm);
            float source = sse_convert_bits_to_float(sse_convert_read_scalar_float_bits(ctx, &inst));
            int64_t integer_value = 0;
            bool success = sse_convert_round_float_to_integer(source, dest_bits, truncate, ctx->mxcsr, &integer_value);
            sse_convert_write_integer_destination(ctx, dest, dest_bits, success, integer_value);
            return;
        }
        if (mandatory_prefix == 3) {
            int dest = decode_sse2_scalar_convert_gpr_reg_index(ctx, inst.modrm);
            double source = sse2_convert_bits_to_double(sse2_scalar_convert_read_scalar_double_bits(ctx, &inst));
            int64_t integer_value = 0;
            bool success = sse2_convert_round_fp_to_integer(source, dest_bits, truncate, ctx->mxcsr, &integer_value);
            sse2_scalar_convert_write_integer_destination(ctx, dest, dest_bits, success, integer_value);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0xC2) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        uint8_t predicate = (uint8_t)inst.immediate;
        if (mandatory_prefix == 0) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_cmpps256(lhs, rhs, predicate));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_cmp_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_cmpps128(lhs, rhs, predicate));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_cmppd256(lhs, rhs, predicate));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_cmp_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_cmppd128(lhs, rhs, predicate));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 2) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_cmpss128(src1, read_sse_cmp_scalar_source_bits(ctx, &inst), predicate));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_cmpsd128(src1, read_sse2_cmp_pd_scalar_source_bits(ctx, &inst), predicate));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x2E || opcode == 0x2F) {
        if (is_256 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            float lhs = sse_cmp_bits_to_float(get_xmm_lane_bits(get_xmm128(ctx, dest), 0));
            float rhs = sse_cmp_bits_to_float(read_sse_cmp_scalar_source_bits(ctx, &inst));
            update_flags_comiss_ucomiss(ctx, lhs, rhs);
            return;
        }
        if (mandatory_prefix == 1) {
            double lhs = sse2_cmp_pd_bits_to_double(get_xmm128(ctx, dest).low);
            double rhs = sse2_cmp_pd_bits_to_double(read_sse2_cmp_pd_scalar_source_bits(ctx, &inst));
            update_flags_comisd_ucomisd(ctx, lhs, rhs);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x60 || opcode == 0x61 || opcode == 0x62 || opcode == 0x63 || opcode == 0x67 || opcode == 0x68 || opcode == 0x69 || opcode == 0x6A || opcode == 0x6B || opcode == 0x6C || opcode == 0x6D) {
        if (mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (is_256) {
            AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx_pack256(ctx, opcode, lhs, rhs));
        }
        else {
            XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            XMMRegister rhs = read_sse2_pack_source_operand(ctx, &inst);
            set_xmm128(ctx, dest, apply_avx_pack128(ctx, opcode, lhs, rhs));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0x64 || opcode == 0x65 || opcode == 0x66 || opcode == 0x74 || opcode == 0x75 || opcode == 0x76) {
        if (mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (is_256) {
            AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx_int_cmp256(ctx, opcode, lhs, rhs));
        }
        else {
            XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            XMMRegister rhs = read_sse2_int_cmp_source_operand(ctx, &inst);
            set_xmm128(ctx, dest, apply_avx_int_cmp128(ctx, opcode, lhs, rhs));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0x70) {
        if (mandatory_prefix < 1 || mandatory_prefix > 3 || avx_vex_requires_reserved_vvvv(&prefix)) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        uint8_t imm8 = (uint8_t)inst.immediate;
        if (is_256) {
            AVXRegister256 source = read_avx_rm256(ctx, &inst);
            if (mandatory_prefix == 1) {
                set_ymm256(ctx, dest, apply_avx_pshufd256(source, imm8));
            }
            else if (mandatory_prefix == 2) {
                set_ymm256(ctx, dest, apply_avx_pshufhw256(source, imm8));
            }
            else {
                set_ymm256(ctx, dest, apply_avx_pshuflw256(source, imm8));
            }
        }
        else {
            XMMRegister source = read_sse2_pack_source_operand(ctx, &inst);
            if (mandatory_prefix == 1) {
                set_xmm128(ctx, dest, apply_avx_pshufd128(source, imm8));
            }
            else if (mandatory_prefix == 2) {
                set_xmm128(ctx, dest, apply_sse2_pshufhw128(source, imm8));
            }
            else {
                set_xmm128(ctx, dest, apply_sse2_pshuflw128(source, imm8));
            }
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0xDA || opcode == 0xDE || opcode == 0xEA || opcode == 0xEE) {
        if (mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (is_256) {
            AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx_int_minmax256(ctx, opcode, lhs, rhs));
        }
        else {
            XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            XMMRegister rhs = read_sse2_pack_source_operand(ctx, &inst);
            set_xmm128(ctx, dest, apply_avx_int_minmax128(ctx, opcode, lhs, rhs));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0xD4 || opcode == 0xD5 || opcode == 0xD8 || opcode == 0xD9 || opcode == 0xDC || opcode == 0xDD || opcode == 0xE0 || opcode == 0xE3 || opcode == 0xE4 || opcode == 0xE5 || opcode == 0xE8 || opcode == 0xE9 || opcode == 0xEC || opcode == 0xED || opcode == 0xF4 || opcode == 0xF5 || opcode == 0xF6 || opcode == 0xF8 || opcode == 0xF9 || opcode == 0xFA || opcode == 0xFB || opcode == 0xFC || opcode == 0xFD || opcode == 0xFE) {
        if (mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (is_256) {
            AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
            set_ymm256(ctx, dest, apply_avx_int_arith256(ctx, opcode, lhs, rhs));
        }
        else {
            XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            XMMRegister rhs = read_sse2_int_arith_source_operand(ctx, &inst);
            set_xmm128(ctx, dest, apply_avx_int_arith128(ctx, opcode, lhs, rhs));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0x71 || opcode == 0x72 || opcode == 0x73 || opcode == 0xD1 || opcode == 0xD2 || opcode == 0xD3 || opcode == 0xE1 || opcode == 0xE2 || opcode == 0xF1 || opcode == 0xF2 || opcode == 0xF3) {
        if (mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        uint64_t count = 0;
        uint8_t group = (inst.modrm >> 3) & 0x07;
        if (opcode == 0x71 || opcode == 0x72 || opcode == 0x73) {
            bool is_shift_dq = opcode == 0x73 && (group == 3 || group == 7);
            int dest = is_shift_dq ? avx_vex_source1_index(&prefix) : avx_vex_rm_index(ctx, inst.modrm);
            count = (uint8_t)inst.immediate;
            if (is_256) {
                AVXRegister256 src = is_shift_dq ? get_ymm256(ctx, avx_vex_rm_index(ctx, inst.modrm))
                                                 : get_ymm256(ctx, avx_vex_source1_index(&prefix));
                set_ymm256(ctx, dest, apply_avx_shift256(ctx, opcode, group, src, count));
            }
            else {
                XMMRegister src = is_shift_dq ? get_xmm128(ctx, avx_vex_rm_index(ctx, inst.modrm))
                                              : get_xmm128(ctx, avx_vex_source1_index(&prefix));
                set_xmm128(ctx, dest, apply_avx_shift128(ctx, opcode, group, src, count));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }

        int dest = avx_vex_dest_index(ctx, inst.modrm);
        XMMRegister count_source = read_sse2_shift_source_operand(ctx, &inst);
        count = count_source.low;
        if (is_256) {
            AVXRegister256 src = get_ymm256(ctx, avx_vex_source1_index(&prefix));
            set_ymm256(ctx, dest, apply_avx_shift256(ctx, opcode, group, src, count));
        }
        else {
            XMMRegister src = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_shift128(ctx, opcode, group, src, count));
            clear_ymm_upper128(ctx, dest);
        }
        return;
    }

    if (opcode == 0x51 || opcode == 0x52 || opcode == 0x53) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_unary_math256(ctx, opcode, read_avx_rm256(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_unary_math128(ctx, opcode, read_sse_math_packed_source(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (opcode != 0x51) {
                raise_ud_ctx(ctx);
            }
            if (avx_vex_requires_reserved_vvvv(&prefix)) {
                raise_ud_ctx(ctx);
            }
            if (is_256) {
                set_ymm256(ctx, dest, apply_avx_sqrt_pd256(read_avx_rm256(ctx, &inst)));
            }
            else {
                set_xmm128(ctx, dest, apply_avx_sqrt_pd128(read_sse2_math_pd_packed_source(ctx, &inst)));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 2) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_unary_math_ss128(ctx, opcode, src1, read_sse_math_scalar_source_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 3) {
            if (opcode != 0x51) {
                raise_ud_ctx(ctx);
            }
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_sqrt_sd128(src1, read_sse2_math_pd_scalar_source_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x5D || opcode == 0x5F) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_minmax256(opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_math_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_minmax128(opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_minmax_pd256(opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_math_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_minmax_pd128(opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 2) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_minmax_ss128(opcode, src1, read_sse_math_scalar_source_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_minmax_sd128(opcode, src1, read_sse2_math_pd_scalar_source_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x58 || opcode == 0x59 || opcode == 0x5C || opcode == 0x5E) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 0) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_arith256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_arith_source_operand(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_arith128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_arith_pd256(ctx, opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_arith_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_arith_pd128(ctx, opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 2) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_arith_ss128(ctx, opcode, src1, sse_convert_read_scalar_float_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                raise_ud_ctx(ctx);
            }
            XMMRegister src1 = get_xmm128(ctx, avx_vex_source1_index(&prefix));
            set_xmm128(ctx, dest, apply_avx_arith_sd128(ctx, opcode, src1, read_sse2_arith_pd_scalar_source_bits(ctx, &inst)));
            clear_ymm_upper128(ctx, dest);
            return;
        }
        raise_ud_ctx(ctx);
    }

    if (opcode == 0x7C || opcode == 0x7D) {
        DecodedInstruction inst = decode_avx_vex_modrm(ctx, code, code_size, &prefix);
        int dest = avx_vex_dest_index(ctx, inst.modrm);
        if (mandatory_prefix == 1) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_haddhsub_pd256(opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse2_math_pd_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_haddhsub_pd128(opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        if (mandatory_prefix == 3) {
            if (is_256) {
                AVXRegister256 lhs = get_ymm256(ctx, avx_vex_source1_index(&prefix));
                AVXRegister256 rhs = read_avx_rm256(ctx, &inst);
                set_ymm256(ctx, dest, apply_avx_haddhsub_ps256(opcode, lhs, rhs));
            }
            else {
                XMMRegister lhs = get_xmm128(ctx, avx_vex_source1_index(&prefix));
                XMMRegister rhs = read_sse_math_packed_source(ctx, &inst);
                set_xmm128(ctx, dest, apply_avx_haddhsub_ps128(opcode, lhs, rhs));
                clear_ymm_upper128(ctx, dest);
            }
            return;
        }
        raise_ud_ctx(ctx);
    }

    raise_ud_ctx(ctx);
}
