#pragma once

// instrusments/aeskeygenassist.hpp - AESKEYGENASSIST/VAESKEYGENASSIST implementation

static int decode_aeskeygenassist_xmm_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

static int decode_aeskeygenassist_xmm_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static void decode_modrm_aeskeygenassist(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
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
}

static const uint8_t AESKEYGENASSIST_SBOX[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

static uint32_t aeskeygenassist_subword_from_bytes(const uint8_t* bytes) {
    return (uint32_t)AESKEYGENASSIST_SBOX[bytes[0]] |
        ((uint32_t)AESKEYGENASSIST_SBOX[bytes[1]] << 8) |
        ((uint32_t)AESKEYGENASSIST_SBOX[bytes[2]] << 16) |
        ((uint32_t)AESKEYGENASSIST_SBOX[bytes[3]] << 24);
}

static uint32_t aeskeygenassist_rotword_subword_from_bytes(const uint8_t* bytes) {
    return (uint32_t)AESKEYGENASSIST_SBOX[bytes[1]] |
        ((uint32_t)AESKEYGENASSIST_SBOX[bytes[2]] << 8) |
        ((uint32_t)AESKEYGENASSIST_SBOX[bytes[3]] << 16) |
        ((uint32_t)AESKEYGENASSIST_SBOX[bytes[0]] << 24);
}

static XMMRegister apply_aeskeygenassist128(XMMRegister source, uint8_t imm8) {
    uint8_t source_bytes[16] = {};
    sse2_pack_xmm_to_bytes(source, source_bytes);

    uint32_t rcon = (uint32_t)imm8;
    uint32_t dest0 = aeskeygenassist_subword_from_bytes(&source_bytes[4]);
    uint32_t dest1 = aeskeygenassist_rotword_subword_from_bytes(&source_bytes[4]) ^ rcon;
    uint32_t dest2 = aeskeygenassist_subword_from_bytes(&source_bytes[12]);
    uint32_t dest3 = aeskeygenassist_rotword_subword_from_bytes(&source_bytes[12]) ^ rcon;

    uint8_t dest_bytes[16] = {};
    dest_bytes[0] = (uint8_t)(dest0 & 0xFF);
    dest_bytes[1] = (uint8_t)((dest0 >> 8) & 0xFF);
    dest_bytes[2] = (uint8_t)((dest0 >> 16) & 0xFF);
    dest_bytes[3] = (uint8_t)((dest0 >> 24) & 0xFF);
    dest_bytes[4] = (uint8_t)(dest1 & 0xFF);
    dest_bytes[5] = (uint8_t)((dest1 >> 8) & 0xFF);
    dest_bytes[6] = (uint8_t)((dest1 >> 16) & 0xFF);
    dest_bytes[7] = (uint8_t)((dest1 >> 24) & 0xFF);
    dest_bytes[8] = (uint8_t)(dest2 & 0xFF);
    dest_bytes[9] = (uint8_t)((dest2 >> 8) & 0xFF);
    dest_bytes[10] = (uint8_t)((dest2 >> 16) & 0xFF);
    dest_bytes[11] = (uint8_t)((dest2 >> 24) & 0xFF);
    dest_bytes[12] = (uint8_t)(dest3 & 0xFF);
    dest_bytes[13] = (uint8_t)((dest3 >> 8) & 0xFF);
    dest_bytes[14] = (uint8_t)((dest3 >> 16) & 0xFF);
    dest_bytes[15] = (uint8_t)((dest3 >> 24) & 0xFF);
    return sse2_pack_bytes_to_xmm(dest_bytes);
}

static XMMRegister read_aeskeygenassist_source_operand(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    if (((inst->modrm >> 6) & 0x03) == 0x03) {
        return get_xmm128(ctx, decode_aeskeygenassist_xmm_rm_index(ctx, inst->modrm));
    }
    return read_xmm_memory(ctx, inst->mem_address);
}

inline bool is_aeskeygenassist_instruction(const uint8_t* code, int len, int prefix_len) {
    if (!code || prefix_len + 4 >= len) {
        return false;
    }
    return code[prefix_len] == 0x0F && code[prefix_len + 1] == 0x3A && code[prefix_len + 2] == 0xDF;
}

inline DecodedInstruction decode_aeskeygenassist_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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
        uint8_t prefix = code[offset];
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
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E || prefix == 0x64 || prefix == 0x65) {
            offset++;
        }
        else {
            break;
        }
    }

    if (has_lock_prefix || has_unsupported_simd_prefix || mandatory_prefix != 0x66 || !cpu_has_aes_feature()) {
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

    inst.opcode = code[offset++];
    inst.mandatory_prefix = mandatory_prefix;
    if (inst.opcode != 0xDF) {
        raise_ud_ctx(ctx);
        return inst;
    }

    if (ctx->cs.descriptor.long_mode) {
        inst.address_size = ctx->address_size_override ? 32 : 64;
    }
    else {
        inst.address_size = ctx->address_size_override ? 16 : 32;
    }

    decode_modrm_aeskeygenassist(ctx, &inst, code, code_size, &offset);
    if (cpu_has_exception(ctx)) {
        return inst;
    }

    inst.imm_size = 1;
    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return inst;
    }
    inst.immediate = code[offset++];

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, inst.inst_size);
    ctx->last_inst_size = inst.inst_size;
    return inst;
}

inline void execute_aeskeygenassist(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_aeskeygenassist_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }

    int dest = decode_aeskeygenassist_xmm_reg_index(ctx, inst.modrm);
    XMMRegister source = read_aeskeygenassist_source_operand(ctx, &inst);
    set_xmm128(ctx, dest, apply_aeskeygenassist128(source, (uint8_t)inst.immediate));
}
