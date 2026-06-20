#pragma once

// instructions/bmi.hpp - BMI1/BMI2 VEX-encoded integer instructions.

#include "avx_vex_common.hpp"

static inline bool cpu_has_bmi1_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 3)) != 0;
}

static inline bool cpu_has_bmi2_feature() {
    int cpu_info[4] = {};
    cpu_query_cpuid(cpu_info, 0, 0);
    if (cpu_info[0] < 7) {
        return false;
    }
    cpu_query_cpuid(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 8)) != 0;
}

static inline int bmi_reg_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int reg = (modrm >> 3) & 0x07;
    if (ctx->rex_r) {
        reg |= 0x08;
    }
    return reg;
}

static inline int bmi_rm_index(CPU_CONTEXT* ctx, uint8_t modrm) {
    int rm = modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }
    return rm;
}

static inline int bmi_vex_index(const AVXVexPrefix* prefix) {
    return avx_vex_source1_index(prefix);
}

static inline int bmi_operand_size(CPU_CONTEXT* ctx) {
    return (ctx->cs.descriptor.long_mode && ctx->rex_w) ? 64 : 32;
}

static inline uint64_t bmi_operand_mask(int operand_size) {
    return operand_size == 64 ? 0xFFFFFFFFFFFFFFFFULL : 0xFFFFFFFFULL;
}

static inline uint64_t bmi_sign_mask(int operand_size) {
    return operand_size == 64 ? 0x8000000000000000ULL : 0x80000000ULL;
}

static inline void decode_bmi_modrm(CPU_CONTEXT* ctx, DecodedInstruction* inst, uint8_t* code, size_t code_size, size_t* offset) {
    if (*offset >= code_size) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    inst->has_modrm = true;
    inst->modrm = code[(*offset)++];

    const uint8_t mod = (inst->modrm >> 6) & 0x03;
    const uint8_t rm = inst->modrm & 0x07;

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
        for (int index = 0; index < inst->disp_size; index++) {
            inst->displacement |= ((int32_t)code[(*offset)++]) << (index * 8);
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

static inline DecodedInstruction decode_bmi_vex_modrm(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, const AVXVexPrefix* prefix) {
    DecodedInstruction inst = {};
    inst.opcode = prefix->opcode;
    inst.address_size = ctx->cs.descriptor.long_mode ? 64 : 32;
    inst.operand_size = bmi_operand_size(ctx);
    size_t offset = prefix->modrm_offset;
    decode_bmi_modrm(ctx, &inst, code, code_size, &offset);
    inst.inst_size = (int)offset;
    finalize_rip_relative_address(ctx, &inst, (int)offset);
    ctx->last_inst_size = (int)offset;
    return inst;
}

static inline uint64_t bmi_read_rm(CPU_CONTEXT* ctx, const DecodedInstruction* inst) {
    const uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3) {
        const int rm = bmi_rm_index(ctx, inst->modrm);
        return inst->operand_size == 64 ? get_reg64(ctx, rm) : get_reg32(ctx, rm);
    }
    return inst->operand_size == 64 ? read_memory_qword(ctx, inst->mem_address) : read_memory_dword(ctx, inst->mem_address);
}

static inline uint64_t bmi_read_vex_reg(CPU_CONTEXT* ctx, const AVXVexPrefix* prefix, int operand_size) {
    const int reg = bmi_vex_index(prefix);
    return operand_size == 64 ? get_reg64(ctx, reg) : get_reg32(ctx, reg);
}

static inline void bmi_write_reg(CPU_CONTEXT* ctx, int reg, int operand_size, uint64_t value) {
    if (operand_size == 64) {
        set_reg64(ctx, reg, value);
    }
    else {
        set_reg32(ctx, reg, (uint32_t)value);
    }
}

static inline void bmi_update_zf_sf(CPU_CONTEXT* ctx, uint64_t result, int operand_size) {
    const uint64_t mask = bmi_operand_mask(operand_size);
    const uint64_t value = result & mask;
    set_flag(ctx, RFLAGS_ZF, value == 0);
    set_flag(ctx, RFLAGS_SF, (value & bmi_sign_mask(operand_size)) != 0);
}

static inline uint64_t bmi_rotate_right(uint64_t value, uint8_t count, int operand_size) {
    const uint64_t mask = bmi_operand_mask(operand_size);
    const unsigned int width = (unsigned int)operand_size;
    const unsigned int amount = count & (uint8_t)(width - 1);
    value &= mask;
    if (amount == 0) {
        return value;
    }
    return ((value >> amount) | (value << (width - amount))) & mask;
}

static inline uint64_t bmi_bextr(uint64_t source, uint64_t control, int operand_size) {
    const unsigned int start = (unsigned int)(control & 0xFFU);
    const unsigned int length = (unsigned int)((control >> 8) & 0xFFU);
    if (length == 0 || start >= (unsigned int)operand_size) {
        return 0;
    }
    const unsigned int available = (unsigned int)operand_size - start;
    const unsigned int used_length = length < available ? length : available;
    const uint64_t length_mask = used_length >= 64 ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << used_length) - 1ULL);
    return (source >> start) & length_mask;
}

static inline uint64_t bmi_bzhi(uint64_t source, uint64_t control, int operand_size, bool* out_cf) {
    const unsigned int index = (unsigned int)(control & 0xFFU);
    *out_cf = index >= (unsigned int)operand_size;
    if (*out_cf) {
        return source & bmi_operand_mask(operand_size);
    }
    if (index == 0) {
        return 0;
    }
    return source & ((1ULL << index) - 1ULL);
}

static inline uint64_t bmi_pext(uint64_t source, uint64_t mask) {
    uint64_t result = 0;
    uint64_t out_bit = 1;
    while (mask != 0) {
        const uint64_t lowest = mask & (~mask + 1ULL);
        if ((source & lowest) != 0) {
            result |= out_bit;
        }
        mask &= (mask - 1ULL);
        out_bit <<= 1;
    }
    return result;
}

static inline uint64_t bmi_pdep(uint64_t source, uint64_t mask) {
    uint64_t result = 0;
    uint64_t in_bit = 1;
    while (mask != 0) {
        const uint64_t lowest = mask & (~mask + 1ULL);
        if ((source & in_bit) != 0) {
            result |= lowest;
        }
        mask &= (mask - 1ULL);
        in_bit <<= 1;
    }
    return result;
}

static inline void bmi_set_logic_flags(CPU_CONTEXT* ctx, uint64_t result, int operand_size) {
    bmi_update_zf_sf(ctx, result, operand_size);
    set_flag(ctx, RFLAGS_OF, false);
    set_flag(ctx, RFLAGS_AF, false);
    set_flag(ctx, RFLAGS_PF, calc_parity((uint8_t)result));
}

static inline bool try_execute_bmi_vex(CPU_CONTEXT* ctx, uint8_t* code, size_t code_size, const AVXVexPrefix* prefix) {
    const uint8_t opcode = prefix->opcode;
    const uint8_t map_select = avx_vex_map_select(prefix);
    const uint8_t mandatory_prefix = avx_vex_mandatory_prefix(prefix);
    const bool is_256 = avx_vex_is_256(prefix);

    if (map_select == 0x03 && opcode == 0xF0 && mandatory_prefix == 3) {
        if (is_256 || avx_vex_requires_reserved_vvvv(prefix) || !cpu_has_bmi2_feature()) {
            raise_ud_ctx(ctx);
            return true;
        }
        DecodedInstruction inst = decode_bmi_vex_modrm(ctx, code, code_size, prefix);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        size_t offset = (size_t)inst.inst_size;
        if (offset >= code_size) {
            raise_gp_ctx(ctx, 0);
            return true;
        }
        const uint8_t imm8 = code[offset++];
        inst.inst_size = (int)offset;
        ctx->last_inst_size = (int)offset;
        const uint64_t source = bmi_read_rm(ctx, &inst);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), inst.operand_size, bmi_rotate_right(source, imm8, inst.operand_size));
        return true;
    }

    if (map_select != 0x02) {
        return false;
    }

    if (opcode == 0xF7 && (mandatory_prefix == 0 || mandatory_prefix == 1 || mandatory_prefix == 2 || mandatory_prefix == 3)) {
        const bool is_bextr = mandatory_prefix == 0;
        if (is_256 || (is_bextr ? !cpu_has_bmi1_feature() : !cpu_has_bmi2_feature())) {
            raise_ud_ctx(ctx);
            return true;
        }
        DecodedInstruction inst = decode_bmi_vex_modrm(ctx, code, code_size, prefix);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        const uint64_t source = bmi_read_rm(ctx, &inst);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        const uint64_t control = bmi_read_vex_reg(ctx, prefix, inst.operand_size);
        uint64_t result = 0;
        if (mandatory_prefix == 0) {
            result = bmi_bextr(source, control, inst.operand_size);
            bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), inst.operand_size, result);
            set_flag(ctx, RFLAGS_ZF, (result & bmi_operand_mask(inst.operand_size)) == 0);
            set_flag(ctx, RFLAGS_CF, false);
            set_flag(ctx, RFLAGS_OF, false);
            set_flag(ctx, RFLAGS_SF, false);
            set_flag(ctx, RFLAGS_AF, false);
            set_flag(ctx, RFLAGS_PF, false);
            return true;
        }
        const unsigned int shift = (unsigned int)(control & (inst.operand_size == 64 ? 0x3FU : 0x1FU));
        if (mandatory_prefix == 1) {
            result = (source << shift) & bmi_operand_mask(inst.operand_size);
        }
        else if (mandatory_prefix == 3) {
            result = (source >> shift) & bmi_operand_mask(inst.operand_size);
        }
        else {
            if (inst.operand_size == 64) {
                result = (uint64_t)((int64_t)source >> shift);
            }
            else {
                result = (uint32_t)((int32_t)(uint32_t)source >> shift);
            }
        }
        bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), inst.operand_size, result);
        return true;
    }

    if (opcode == 0xF5 && (mandatory_prefix == 0 || mandatory_prefix == 2 || mandatory_prefix == 3)) {
        if (is_256 || !cpu_has_bmi2_feature()) {
            raise_ud_ctx(ctx);
            return true;
        }
        DecodedInstruction inst = decode_bmi_vex_modrm(ctx, code, code_size, prefix);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        const uint64_t rm_value = bmi_read_rm(ctx, &inst) & bmi_operand_mask(inst.operand_size);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        const uint64_t vex_value = bmi_read_vex_reg(ctx, prefix, inst.operand_size) & bmi_operand_mask(inst.operand_size);
        uint64_t result = 0;
        if (mandatory_prefix == 0) {
            bool cf = false;
            result = bmi_bzhi(rm_value, vex_value, inst.operand_size, &cf);
            bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), inst.operand_size, result);
            set_flag(ctx, RFLAGS_ZF, (result & bmi_operand_mask(inst.operand_size)) == 0);
            set_flag(ctx, RFLAGS_CF, cf);
            set_flag(ctx, RFLAGS_OF, false);
            set_flag(ctx, RFLAGS_SF, false);
            set_flag(ctx, RFLAGS_AF, false);
            set_flag(ctx, RFLAGS_PF, false);
            return true;
        }
        if (mandatory_prefix == 2) {
            result = bmi_pext(vex_value, rm_value);
        }
        else {
            result = bmi_pdep(vex_value, rm_value);
        }
        bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), inst.operand_size, result & bmi_operand_mask(inst.operand_size));
        return true;
    }

    if (opcode == 0xF3 && mandatory_prefix == 0) {
        if (is_256 || !cpu_has_bmi1_feature()) {
            raise_ud_ctx(ctx);
            return true;
        }
        DecodedInstruction inst = decode_bmi_vex_modrm(ctx, code, code_size, prefix);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        const uint8_t ext = (inst.modrm >> 3) & 0x07;
        if (ext != 1 && ext != 2 && ext != 3) {
            return false;
        }
        const uint64_t source = bmi_read_rm(ctx, &inst) & bmi_operand_mask(inst.operand_size);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        uint64_t result = 0;
        if (ext == 1) {
            result = source & (source - 1ULL); // BLSR
            set_flag(ctx, RFLAGS_CF, source == 0);
        }
        else if (ext == 2) {
            result = source ^ (source - 1ULL); // BLSMSK
            set_flag(ctx, RFLAGS_CF, source == 0);
        }
        else {
            result = source & (~source + 1ULL); // BLSI
            set_flag(ctx, RFLAGS_CF, source != 0);
        }
        result &= bmi_operand_mask(inst.operand_size);
        bmi_write_reg(ctx, bmi_vex_index(prefix), inst.operand_size, result);
        bmi_set_logic_flags(ctx, result, inst.operand_size);
        return true;
    }

    if (opcode == 0xF6 && mandatory_prefix == 3) {
        if (is_256 || !cpu_has_bmi2_feature()) {
            raise_ud_ctx(ctx);
            return true;
        }
        DecodedInstruction inst = decode_bmi_vex_modrm(ctx, code, code_size, prefix);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        const uint64_t source = bmi_read_rm(ctx, &inst) & bmi_operand_mask(inst.operand_size);
        if (cpu_has_exception(ctx)) {
            return true;
        }
        if (inst.operand_size == 64) {
            uint64_t high = 0;
            const uint64_t low = _umul128(get_reg64(ctx, REG_RDX), source, &high);
            bmi_write_reg(ctx, bmi_vex_index(prefix), 64, low);
            bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), 64, high);
        }
        else {
            const uint64_t product = (uint64_t)get_reg32(ctx, REG_RDX) * (uint64_t)(uint32_t)source;
            bmi_write_reg(ctx, bmi_vex_index(prefix), 32, (uint32_t)product);
            bmi_write_reg(ctx, bmi_reg_index(ctx, inst.modrm), 32, (uint32_t)(product >> 32));
        }
        return true;
    }

    return false;
}
