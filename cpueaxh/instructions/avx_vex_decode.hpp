#pragma once

// instructions/avx_vex_decode.hpp - VEX ModRM / immediate decode helpers.

#include "avx_vex_common.hpp"

static DecodedInstruction decode_avx_vex_modrm(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, const AVXVexPrefix* prefix) {
    DecodedInstruction inst = {};
    uint8_t opcode = prefix->opcode;
    uint8_t mandatory_prefix = avx_vex_mandatory_prefix(prefix);
    uint8_t map_select = avx_vex_map_select(prefix);
    inst.opcode = opcode;
    inst.address_size = ctx->cs.descriptor.long_mode ? 64 : 32;

    size_t offset = prefix->modrm_offset;
    switch (opcode) {
    case 0x0E:
    case 0x0F:
        decode_modrm_sse2_logic_pd(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x00:
        if (map_select != 0x02 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x0C:
    case 0x0D:
    case 0x10:
    case 0x11:
        if (mandatory_prefix == 1 || mandatory_prefix == 3) {
            decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_movups(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x12:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x50:
        if (map_select == 0x01 && mandatory_prefix == 0) {
            decode_modrm_sse_mov_misc(ctx, &inst, code, code_size, &offset, false);
        }
        else if (opcode == 0x16 && map_select == 0x02 && mandatory_prefix == 1) {
            decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x28:
    case 0x29:
        if (map_select == 0x02 && mandatory_prefix == 1) {
            if (opcode == 0x28) {
                decode_modrm_sse2_int_arith(ctx, &inst, code, code_size, &offset, false);
            }
            else {
                decode_modrm_sse2_int_cmp(ctx, &inst, code, code_size, &offset, false);
            }
        }
        else if (mandatory_prefix == 1) {
            decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_movaps(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
        if (mandatory_prefix == 1) {
            decode_modrm_sse2_logic_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_logic(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x6F:
    case 0x7F:
    case 0xD7:
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x6E:
    case 0x7E:
        decode_modrm_movdq(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0xD0:
        if (mandatory_prefix == 1) {
            decode_modrm_sse2_math_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else if (mandatory_prefix == 3) {
            decode_modrm_sse_math(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            raise_ud_ctx(ctx);
        }
        break;
    case 0xAE:
        decode_sse_state_modrm(ctx, &inst, code, code_size, &offset);
        break;
    case 0xF0:
    case 0xF7:
        decode_modrm_sse2_misc(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0xDB:
    case 0xDF:
    case 0xEB:
    case 0xEF:
        decode_modrm_sse2_int_logic(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6C:
    case 0x6D:
    case 0xDA:
    case 0xDE:
    case 0xEA:
    case 0xEE:
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x37:
        if (map_select != 0x02 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_int_cmp(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
        if (map_select != 0x02 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x99:
    case 0xA9:
    case 0xB9:
        if (map_select != 0x02 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_arith_pd(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x8C:
    case 0x8E:
        if (map_select != 0x02 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x74:
    case 0x75:
    case 0x76:
        decode_modrm_sse2_int_cmp(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x70:
        decode_modrm_sse_shuffle(ctx, &inst, code, code_size, &offset, false);
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
        }
        inst.immediate = code[offset++];
        inst.imm_size = 1;
        break;
    case 0xD4:
    case 0xD5:
    case 0x04:
    case 0x0B:
    case 0x40:
    case 0x41:
    case 0xD8:
    case 0xD9:
    case 0xDD:
    case 0xE0:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xF6:
    case 0xE8:
    case 0xE9:
    case 0xEC:
    case 0xED:
    case 0xF4:
    case 0xF5:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
    case 0xFE:
        decode_modrm_sse2_int_arith(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x71:
    case 0x72:
    case 0x73:
    case 0xD1:
    case 0xD2:
    case 0xD3:
    case 0xE1:
    case 0xE2:
    case 0xF1:
    case 0xF2:
    case 0xF3:
        decode_modrm_sse2_shift(ctx, &inst, code, code_size, &offset, false);
        if (opcode == 0x71 || opcode == 0x72) {
            uint8_t group = (inst.modrm >> 3) & 0x07;
            if (((inst.modrm >> 6) & 0x03) != 0x03) {
                raise_ud_ctx(ctx);
            }
            if (group != 2 && group != 4 && group != 6) {
                raise_ud_ctx(ctx);
            }
            if (offset >= code_size) {
                raise_gp_ctx(ctx, 0);
            }
            inst.immediate = code[offset++];
            inst.imm_size = 1;
        }
        else if (opcode == 0x73) {
            uint8_t group = (inst.modrm >> 3) & 0x07;
            if (((inst.modrm >> 6) & 0x03) != 0x03) {
                raise_ud_ctx(ctx);
            }
            if (group != 2 && group != 3 && group != 6 && group != 7) {
                raise_ud_ctx(ctx);
            }
            if (offset >= code_size) {
                raise_gp_ctx(ctx, 0);
            }
            inst.immediate = code[offset++];
            inst.imm_size = 1;
        }
        break;
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x5D:
    case 0x5F:
        if (mandatory_prefix == 1 || mandatory_prefix == 3) {
            decode_modrm_sse2_math_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_math(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x78:
    case 0x79:
        if (map_select != 0x02 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x7C:
    case 0x7D:
        if (mandatory_prefix == 1) {
            decode_modrm_sse2_math_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else if (mandatory_prefix == 3) {
            decode_modrm_sse_math(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            raise_ud_ctx(ctx);
        }
        break;
    case 0x58:
    case 0x59:
        if (map_select == 0x02 && mandatory_prefix == 1) {
            decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else if (mandatory_prefix == 1 || mandatory_prefix == 3) {
            decode_modrm_sse2_arith_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_arith(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x5C:
    case 0x5E:
        if (mandatory_prefix == 1 || mandatory_prefix == 3) {
            decode_modrm_sse2_arith_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_arith(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x5A:
        if (map_select == 0x02 && mandatory_prefix == 1) {
            decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse2_convert(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x5B:
    case 0xE6:
        decode_modrm_sse2_convert(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0xE7:
        if (map_select != 0x01 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x2A:
        if (map_select == 0x02 && mandatory_prefix == 1) {
            decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_convert(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    case 0x2B:
        if (map_select == 0x02 && mandatory_prefix == 1) {
            decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            if (map_select != 0x01) {
                raise_ud_ctx(ctx);
            }
            if (mandatory_prefix == 1) {
                decode_modrm_sse2_mov_pd(ctx, &inst, code, code_size, &offset, false);
            }
            else {
                decode_modrm_movups(ctx, &inst, code, code_size, &offset, false);
            }
        }
        break;
    case 0x2C:
    case 0x2D:
        decode_modrm_sse_convert(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x14:
    case 0x15:
        decode_modrm_sse_shuffle(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0xC6:
        decode_modrm_sse_shuffle(ctx, &inst, code, code_size, &offset, false);
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
        }
        inst.immediate = code[offset++];
        inst.imm_size = 1;
        break;
    case 0xC4:
        if (map_select != 0x01 || mandatory_prefix != 1) {
            raise_ud_ctx(ctx);
        }
        decode_modrm_sse2_mov_int(ctx, &inst, code, code_size, &offset, false);
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
        }
        inst.immediate = code[offset++];
        inst.imm_size = 1;
        break;
    case 0xC2:
        if (mandatory_prefix == 1 || mandatory_prefix == 3) {
            decode_modrm_sse2_cmp_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_cmp(ctx, &inst, code, code_size, &offset, false);
        }
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
        }
        inst.immediate = code[offset++];
        inst.imm_size = 1;
        break;
    case 0x2E:
    case 0x2F:
        if (mandatory_prefix == 1) {
            decode_modrm_sse2_cmp_pd(ctx, &inst, code, code_size, &offset, false);
        }
        else {
            decode_modrm_sse_cmp(ctx, &inst, code, code_size, &offset, false);
        }
        break;
    default:
        raise_ud_ctx(ctx);
        break;
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static DecodedInstruction decode_avx_vex_0f3a_modrm_imm(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, const AVXVexPrefix* prefix) {
    DecodedInstruction inst = {};
    uint8_t opcode = prefix->opcode;
    uint8_t mandatory_prefix = avx_vex_mandatory_prefix(prefix);
    inst.opcode = opcode;
    inst.address_size = ctx->cs.descriptor.long_mode ? 64 : 32;

    if (mandatory_prefix != 1) {
        raise_ud_ctx(ctx);
    }

    size_t offset = prefix->modrm_offset;
    switch (opcode) {
    case 0x01:
    case 0x02:
    case 0x04:
    case 0x06:
    case 0x08:
    case 0x0A:
    case 0x00:
    case 0x0C:
    case 0x0E:
    case 0x0F:
    case 0x21:
    case 0x40:
    case 0x4A:
    case 0x4C:
        decode_modrm_sse_arith(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x17:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x20:
    case 0x22:
    case 0xDF:
    case 0x42:
        decode_modrm_sse2_mov_int(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
        decode_modrm_sse2_pack(ctx, &inst, code, code_size, &offset, false);
        break;
    case 0x05:
    case 0x09:
    case 0x0B:
    case 0x0D:
    case 0x18:
    case 0x19:
    case 0x38:
    case 0x39:
    case 0x41:
    case 0x46:
    case 0x4B:
        decode_modrm_sse2_arith_pd(ctx, &inst, code, code_size, &offset, false);
        break;
    default:
        raise_ud_ctx(ctx);
    }

    if (offset >= code_size) {
        raise_gp_ctx(ctx, 0);
    }

    inst.immediate = code[offset++];
    inst.imm_size = 1;
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

