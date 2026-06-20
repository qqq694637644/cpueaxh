#pragma once

// cpu/dispatch_helpers.hpp - Stateless instruction-byte inspection helpers
// shared by the decoder and the executor.
//
// These previously lived as static functions inside executor.hpp; they were
// hoisted here so both the legacy (per-step) decode path in the executor and
// the new cache-aware decoder can share them.

#pragma once

#include "../cpueaxh_platform.hpp"
#include "def.h"
#include "helper.hpp"

#define MAX_INST_FETCH 15

inline uint16_t peek_opcode(const CPU_CONTEXT* ctx, const uint8_t* buf, int len, int* prefix_len) {
    int i = 0;
    while (i < len) {
        uint8_t b = buf[i];
        if (b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E ||
            b == 0x64 || b == 0x65 ||
            b == 0x66 || b == 0x67 ||
            b == 0xF0 || b == 0xF2 || b == 0xF3) {
            i++; continue;
        }
        if (cpu_allows_rex_prefix(ctx) && b >= 0x40 && b <= 0x4F) { i++; continue; }
        break;
    }
    *prefix_len = i;
    if (i >= len) return 0xFFFF;
    uint8_t opc = buf[i];
    if (opc == 0x0F && i + 1 < len)
        return (uint16_t)(0x0F00 | buf[i + 1]);
    return opc;
}

inline uint8_t peek_repeat_prefix(const uint8_t* buf, int prefix_len) {
    uint8_t repeat_prefix = 0;
    for (int i = 0; i < prefix_len; i++) {
        if (buf[i] == 0xF2 || buf[i] == 0xF3) {
            repeat_prefix = buf[i];
        }
    }
    return repeat_prefix;
}

inline uint8_t peek_mandatory_prefix(const uint8_t* buf, int prefix_len) {
    uint8_t mandatory_prefix = 0;
    for (int i = 0; i < prefix_len; i++) {
        if (buf[i] == 0x66 || buf[i] == 0xF2 || buf[i] == 0xF3) {
            mandatory_prefix = buf[i];
        }
    }
    return mandatory_prefix;
}

struct InstructionPrefixPresence {
    bool has_66;
    bool has_f2;
    bool has_f3;
    bool has_lock;
};

inline InstructionPrefixPresence peek_instruction_prefix_presence(const uint8_t* buf, int prefix_len) {
    InstructionPrefixPresence prefixes = {};
    for (int i = 0; i < prefix_len; i++) {
        prefixes.has_66 = prefixes.has_66 || buf[i] == 0x66;
        prefixes.has_f2 = prefixes.has_f2 || buf[i] == 0xF2;
        prefixes.has_f3 = prefixes.has_f3 || buf[i] == 0xF3;
        prefixes.has_lock = prefixes.has_lock || buf[i] == 0xF0;
    }
    return prefixes;
}

struct LegacyPrefixState {
    bool has_66;
    bool has_67;
    bool has_f2;
    bool has_f3;
    bool has_lock;
};

inline LegacyPrefixState decode_legacy_prefixes(CPU_CONTEXT* ctx, const uint8_t* code, size_t code_size, size_t* offset) {
    LegacyPrefixState prefixes = {};
    if (!ctx || !code || !offset) {
        return prefixes;
    }

    cpu_reset_prefix_state(ctx);
    while (*offset < code_size) {
        const uint8_t prefix = code[*offset];
        if (prefix == 0x66) {
            prefixes.has_66 = true;
            ctx->operand_size_override = true;
            (*offset)++;
        }
        else if (prefix == 0x67) {
            prefixes.has_67 = true;
            ctx->address_size_override = true;
            (*offset)++;
        }
        else if (prefix == 0xF2) {
            prefixes.has_f2 = true;
            (*offset)++;
        }
        else if (prefix == 0xF3) {
            prefixes.has_f3 = true;
            (*offset)++;
        }
        else if (cpu_try_apply_rex_prefix(ctx, prefix)) {
            (*offset)++;
        }
        else if (prefix == 0xF0) {
            prefixes.has_lock = true;
            (*offset)++;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65) {
            (*offset)++;
        }
        else {
            break;
        }
    }
    return prefixes;
}

inline void decode_modrm_common(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset, bool allow_register_form) {
    if (!ctx || !inst || !code || !offset || *offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    const uint8_t mod = (inst->modrm >> 6) & 0x03;
    const uint8_t rm = inst->modrm & 0x07;
    if (mod == 3 && !allow_register_form) {
        raise_ud_ctx(ctx);
        return;
    }

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

inline int peek_segment_override(const uint8_t* buf, int prefix_len) {
    int segment_override = -1;
    for (int i = 0; i < prefix_len; i++) {
        int decoded_segment = cpu_decode_segment_override(buf[i]);
        if (decoded_segment >= 0) {
            segment_override = decoded_segment;
        }
    }
    return segment_override;
}

inline bool is_branch_opcode(uint16_t opc) {
    if (opc >= 0x0F80 && opc <= 0x0F8F) return true;
    if (opc > 0x00FF) return false;
    switch (opc & 0xFF) {
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B:
    case 0x7C: case 0x7D: case 0x7E: case 0x7F:
    case 0xE0: case 0xE1: case 0xE2: case 0xE3:
    case 0xEB: case 0xE9: case 0xEA:
    case 0xE8: case 0x9A:
    case 0xC2: case 0xC3: case 0xCA: case 0xCB:
    case 0xCF:
    case 0xFF:
        return true;
    default:
        return false;
    }
}

inline uint8_t peek_modrm_reg_field(const uint8_t* buf, int fetched, int prefix_len, int opc_offset) {
    int pos = prefix_len + opc_offset;
    return (pos < fetched) ? (uint8_t)((buf[pos] >> 3) & 0x07) : 0xFF;
}

// Returns the ModRM mod field (high 2 bits) at the given offset past the
// opcode, or 0xFF when the buffer is too short. Used by the decoder to tell
// register-form encodings (mod == 0b11) from memory operands.
inline uint8_t peek_modrm_mod_field(const uint8_t* buf, int fetched, int prefix_len, int opc_offset) {
    int pos = prefix_len + opc_offset;
    return (pos < fetched) ? (uint8_t)((buf[pos] >> 6) & 0x03) : 0xFF;
}

inline int decode_near_ret_operand_size(CPU_CONTEXT* ctx, const uint8_t* buf, int fetched, int* prefix_len_out) {
    int prefix_len = 0;
    bool operand_size_override = false;
    while (prefix_len < fetched) {
        const uint8_t prefix = buf[prefix_len];
        if (prefix == 0x66) {
            operand_size_override = true;
            prefix_len++;
            continue;
        }
        if (prefix == 0x67 || prefix == 0xF0 || prefix == 0xF2 || prefix == 0xF3 ||
            prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
            prefix == 0x64 || prefix == 0x65) {
            prefix_len++;
            continue;
        }
        if (cpu_allows_rex_prefix(ctx) && prefix >= 0x40 && prefix <= 0x4F) {
            prefix_len++;
            continue;
        }
        break;
    }

    if (prefix_len_out) {
        *prefix_len_out = prefix_len;
    }
    if (cpu_is_64bit_code(ctx)) {
        return operand_size_override ? 16 : 64;
    }
    return operand_size_override ? 16 : 32;
}

inline bool cpu_step_may_touch_vector_state(const uint8_t* buf, int fetched, int prefix_len, uint16_t opc, uint8_t mandatory_prefix) {
    if (fetched <= 0 || !buf) {
        return false;
    }

    if (buf[0] == 0xC5 || buf[0] == 0xC4) {
        return true;
    }

    if (opc <= 0x00FF) {
        return false;
    }

    if ((opc >= 0x0F40 && opc <= 0x0F4F) ||
        (opc >= 0x0F80 && opc <= 0x0F8F) ||
        (opc >= 0x0F90 && opc <= 0x0F9F) ||
        opc == 0x0FA0 || opc == 0x0FA1 || opc == 0x0FA2 || opc == 0x0FA8 || opc == 0x0FA9 ||
        opc == 0x0FAF ||
        opc == 0x0FB0 || opc == 0x0FB1 || opc == 0x0FB6 || opc == 0x0FB7 || opc == 0x0FBC || opc == 0x0FBD || opc == 0x0FBE || opc == 0x0FBF ||
        opc == 0x0FA3 || opc == 0x0FAB || opc == 0x0FB3 || opc == 0x0FBB || opc == 0x0FBA ||
        opc == 0x0FC0 || opc == 0x0FC1 ||
        (opc >= 0x0FC8 && opc <= 0x0FCF) ||
        opc == 0x0F00 || opc == 0x0F01 || opc == 0x0F05 || opc == 0x0F20 || opc == 0x0F22 || opc == 0x0F31 || opc == 0x0F34 || opc == 0x0F1F) {
        return false;
    }

    if (opc == 0x0FC7 || opc == 0x0F1E) {
        return false;
    }

    if ((opc == 0x0F0D || opc == 0x0F18) ||
        (opc == 0x0F2B && mandatory_prefix == 0x00)) {
        return opc == 0x0F2B;
    }

    return true;
}
