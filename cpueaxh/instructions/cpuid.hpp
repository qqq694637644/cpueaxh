#pragma once

// instrusments/cpuid.hpp - CPUID instruction implementation

#include <intrin.h>

struct CPUID_CACHE_ENTRY {
    bool valid;
    uint32_t leaf;
    uint32_t subleaf;
    int values[4];
};

static CPUID_CACHE_ENTRY g_cpuid_cache[64] = {};

static void copy_cpuid_values(int dst[4], const int src[4]) {
    for (int index = 0; index < 4; index++) {
        dst[index] = src[index];
    }
}

static void cpu_query_cpuid(int out[4], uint32_t leaf, uint32_t subleaf) {
    for (int index = 0; index < (int)(sizeof(g_cpuid_cache) / sizeof(g_cpuid_cache[0])); index++) {
        if (!g_cpuid_cache[index].valid) {
            continue;
        }
        if (g_cpuid_cache[index].leaf == leaf && g_cpuid_cache[index].subleaf == subleaf) {
            copy_cpuid_values(out, g_cpuid_cache[index].values);
            return;
        }
    }

    int values[4] = {};
    __cpuidex(values, (int)leaf, (int)subleaf);

    for (int index = 0; index < (int)(sizeof(g_cpuid_cache) / sizeof(g_cpuid_cache[0])); index++) {
        if (g_cpuid_cache[index].valid) {
            continue;
        }
        g_cpuid_cache[index].valid = true;
        g_cpuid_cache[index].leaf = leaf;
        g_cpuid_cache[index].subleaf = subleaf;
        copy_cpuid_values(g_cpuid_cache[index].values, values);
        break;
    }

    copy_cpuid_values(out, values);
}

DecodedInstruction decode_cpuid_instruction(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
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

    if (offset + 2 > code_size) {
        raise_gp_ctx(ctx, 0);
return inst;
    }

    if (code[offset++] != 0x0F) {
        raise_ud_ctx(ctx);
    }

    inst.opcode = code[offset++];
    if (inst.opcode != 0xA2) {
        raise_ud_ctx(ctx);
    }

    if (has_lock_prefix) {
        raise_ud_ctx(ctx);
    }

    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

inline void execute_cpuid_with_decoded(CPU_CONTEXT* ctx, const DecodedInstruction* /*inst_ptr*/) {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, get_reg32(ctx, REG_RAX), get_reg32(ctx, REG_RCX));

    set_reg32(ctx, REG_RAX, (uint32_t)cpu_info[0]);
    set_reg32(ctx, REG_RBX, (uint32_t)cpu_info[1]);
    set_reg32(ctx, REG_RCX, (uint32_t)cpu_info[2]);
    set_reg32(ctx, REG_RDX, (uint32_t)cpu_info[3]);
}

void execute_cpuid(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size) {
    DecodedInstruction inst = decode_cpuid_instruction(ctx, code, code_size);
    if (cpu_has_exception(ctx)) {
        return;
    }
    execute_cpuid_with_decoded(ctx, &inst);
}

inline void execute_cpuid_fast(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    decoded_inst_apply_prefix(ctx, dec);
    ctx->last_inst_size = dec->length;
    execute_cpuid_with_decoded(ctx, &dec->cached);
}
