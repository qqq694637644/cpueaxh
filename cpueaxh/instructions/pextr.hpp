#pragma once

// instrusments/pextr.hpp - PEXTRB/PEXTRD/PEXTRQ implementation

#include "evex.hpp"
#include "movdq.hpp"

static inline bool is_pextr_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 4 >= len) {
        return false;
    }
    if (code[prefix_len] != 0x0F || code[prefix_len + 1] != 0x3A) {
        return false;
    }
    return code[prefix_len + 2] == 0x14 || code[prefix_len + 2] == 0x16;
}

static inline bool is_evex_pextr_instruction(const uint8_t* code, size_t code_size) {
    return code && code_size >= 7 && code[0] == 0x62 && (code[4] == 0x14 || code[4] == 0x16);
}

static inline uint8_t pextr_extract_byte(XMMRegister source, uint8_t imm8) {
    uint8_t source_bytes[16] = {};
    sse2_misc_xmm_to_bytes(source, source_bytes);
    return source_bytes[imm8 & 0x0F];
}

static inline uint32_t pextr_extract_dword(XMMRegister source, uint8_t imm8) {
    return get_xmm_lane_bits(source, imm8 & 0x03);
}

static inline uint64_t pextr_extract_qword(XMMRegister source, uint8_t imm8) {
    return ((imm8 & 0x01) != 0) ? source.high : source.low;
}

static inline void pextr_write_byte_destination(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint8_t value) {
    if (((inst->modrm >> 6) & 0x03) == 0x03) {
        const int dest = decode_movdq_gpr_rm_index(ctx, inst->modrm);
        if (ctx->cs.descriptor.long_mode) {
            set_reg64(ctx, dest, (uint64_t)value);
        }
        else {
            set_reg32(ctx, dest, (uint32_t)value);
        }
        return;
    }
    write_memory_byte(ctx, inst->mem_address, value);
}

static inline void pextr_write_dword_destination(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint32_t value) {
    if (((inst->modrm >> 6) & 0x03) == 0x03) {
        set_reg32(ctx, decode_movdq_gpr_rm_index(ctx, inst->modrm), value);
        return;
    }
    write_memory_dword(ctx, inst->mem_address, value);
}

static inline void pextr_write_qword_destination(CPU_CONTEXT* ctx, const DecodedInstruction* inst, uint64_t value) {
    if (((inst->modrm >> 6) & 0x03) == 0x03) {
        set_reg64(ctx, decode_movdq_gpr_rm_index(ctx, inst->modrm), value);
        return;
    }
    write_memory_qword(ctx, inst->mem_address, value);
}

static inline DecodedInstruction decode_pextr_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = {};
    size_t offset = 0;
    bool has_lock_prefix = false;
    bool has_unsupported_simd_prefix = false;
    uint8_t mandatory_prefix = 0;

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
            if (mandatory_prefix == 0 || mandatory_prefix == prefix) {
                mandatory_prefix = prefix;
            }
            else {
                has_unsupported_simd_prefix = true;
            }
            if (prefix == 0x66) {
                ctx->operand_size_override = true;
            }
            offset++;
        }
        else if (prefix == 0x67) {
            ctx->address_size_override = true;
            offset++;
        }
        else if (prefix >= 0x40 && prefix <= 0x4F) {
            ctx->rex_present = true;
            ctx->rex_w = ((prefix >> 3) & 1) != 0;
            ctx->rex_r = ((prefix >> 2) & 1) != 0;
            ctx->rex_x = ((prefix >> 1) & 1) != 0;
            ctx->rex_b = (prefix & 1) != 0;
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

    if (has_lock_prefix || has_unsupported_simd_prefix || mandatory_prefix != 0x66) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (offset + 5 > code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }

    if (code[offset++] != 0x0F || code[offset++] != 0x3A) {
        raise_ud_ctx(ctx);
        return inst;
    }

    const uint8_t opcode = code[offset++];
    if (opcode != 0x14 && opcode != 0x16) {
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

    inst.opcode = opcode;
    inst.imm_size = 1;
    inst.immediate = code[offset++];
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline void execute_pextr_common(CPU_CONTEXT* ctx, const DecodedInstruction* inst, bool use_qword) {
    const XMMRegister source = get_xmm128(ctx, decode_movdq_xmm_reg_index(ctx, inst->modrm));
    const uint8_t imm8 = (uint8_t)inst->immediate;
    if (inst->opcode == 0x14) {
        pextr_write_byte_destination(ctx, inst, pextr_extract_byte(source, imm8));
        return;
    }

    if (use_qword) {
        pextr_write_qword_destination(ctx, inst, pextr_extract_qword(source, imm8));
        return;
    }

    pextr_write_dword_destination(ctx, inst, pextr_extract_dword(source, imm8));
}

static inline void execute_pextr(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    const DecodedInstruction inst = decode_pextr_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    if (inst.opcode == 0x16 && ctx->rex_w) {
        if (!ctx->cs.descriptor.long_mode) {
            raise_ud_ctx(ctx);
            return;
        }
        execute_pextr_common(ctx, &inst, true);
        return;
    }

    execute_pextr_common(ctx, &inst, false);
}

static inline void execute_evex_pextr(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    EVEXDecodedInstructionInfo info = {};
    const uint8_t opcode = (code && code_size >= 5) ? code[4] : 0;
    if (opcode != 0x14 && opcode != 0x16) {
        raise_ud_ctx(ctx);
        return;
    }

    const DecodedInstruction inst = decode_evex_modrm_imm_instruction(
        ctx,
        code,
        code_size,
        opcode,
        0x03,
        0x01,
        CPUEAXH_EVEX_VL_128,
        false,
        false,
        &info);
    if (cpu_has_exception(ctx)) {
        return;
    }

    if (info.source1 != 0x0F) {
        raise_ud_ctx(ctx);
        return;
    }

    const bool use_qword = opcode == 0x16 && ctx->cs.descriptor.long_mode && ctx->rex_w;
    execute_pextr_common(ctx, &inst, use_qword);
}
