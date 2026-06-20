#pragma once

// cpu/helper.hpp - CPU helper functions (segments, descriptors, register access, addressing)

#include "memory.hpp"

SegmentDescriptor load_descriptor_from_table(CPU_CONTEXT* ctx, uint16_t selector) {
    SegmentDescriptor desc = {};
    uint16_t index = selector >> 3;
    bool ti = (selector >> 2) & 1;

    uint64_t table_base;
    uint16_t table_limit;

    if (ti) {
        table_base = ctx->ldtr_base;
        table_limit = ctx->ldtr_limit;
    }
    else {
        table_base = ctx->gdtr_base;
        table_limit = ctx->gdtr_limit;
    }

    uint64_t desc_offset = (uint64_t)index * 8;
    if (desc_offset + 7 > table_limit) {
        raise_gp_ctx(ctx, selector & 0xFFFC);
        return desc;
    }

    uint64_t desc_addr = table_base + desc_offset;

    uint32_t low = read_memory_dword(ctx, desc_addr);
    uint32_t high = read_memory_dword(ctx, desc_addr + 4);

    desc.base = (low >> 16) | ((high & 0xFF) << 16) | ((high >> 24) << 24);
    desc.limit = (low & 0xFFFF) | ((high & 0x000F0000));
    desc.type = (high >> 8) & 0x0F;
    desc.dpl = (high >> 13) & 0x03;
    desc.present = (high >> 15) & 0x01;
    desc.granularity = (high >> 23) & 0x01;
    desc.db = (high >> 22) & 0x01;
    desc.long_mode = (high >> 21) & 0x01;

    if (desc.granularity) {
        desc.limit = (desc.limit << 12) | 0xFFF;
    }

    return desc;
}

bool is_null_selector(uint16_t selector) {
    return (selector & 0xFFFC) == 0;
}

bool is_data_segment(uint8_t type) {
    return (type & 0x08) == 0;
}

bool is_writable_data_segment(uint8_t type) {
    return is_data_segment(type) && (type & 0x02);
}

bool is_code_segment(uint8_t type) {
    return (type & 0x08) != 0;
}

bool is_readable_code_segment(uint8_t type) {
    return is_code_segment(type) && (type & 0x02);
}

bool is_conforming_code_segment(uint8_t type) {
    return is_code_segment(type) && (type & 0x04);
}

void cpu_reset_prefix_state(CPU_CONTEXT* ctx) {
    if (!ctx) {
        return;
    }
    ctx->rex_present = false;
    ctx->rex_w = false;
    ctx->rex_r = false;
    ctx->rex_x = false;
    ctx->rex_b = false;
    ctx->operand_size_override = false;
    ctx->address_size_override = false;
}

bool cpu_try_apply_rex_prefix(CPU_CONTEXT* ctx, uint8_t prefix) {
    if (!ctx || !cpu_allows_rex_prefix(ctx) || prefix < 0x40 || prefix > 0x4F) {
        return false;
    }
    ctx->rex_present = true;
    ctx->rex_w = ((prefix >> 3) & 1) != 0;
    ctx->rex_r = ((prefix >> 2) & 1) != 0;
    ctx->rex_x = ((prefix >> 1) & 1) != 0;
    ctx->rex_b = (prefix & 1) != 0;
    return true;
}

SegmentRegister* get_segment_register(CPU_CONTEXT* ctx, int index) {
    static SegmentRegister dummy = {};

    switch (index) {
    case SEG_ES: return &ctx->es;
    case SEG_CS: return &ctx->cs;
    case SEG_SS: return &ctx->ss;
    case SEG_DS: return &ctx->ds;
    case SEG_FS: return &ctx->fs;
    case SEG_GS: return &ctx->gs;
    default:
        raise_gp_ctx(ctx, 0);
        return &dummy;
    }
}

bool is_valid_control_register(uint8_t index) {
    return index == REG_CR0 || index == REG_CR2 || index == REG_CR3 || index == REG_CR4 || index == REG_CR8;
}

uint64_t get_control_register(CPU_CONTEXT* ctx, uint8_t index) {
    if (!ctx || !is_valid_control_register(index)) {
        raise_ud_ctx(ctx);
        return 0;
    }

    return ctx->control_regs[index];
}

void set_control_register(CPU_CONTEXT* ctx, uint8_t index, uint64_t value) {
    if (!ctx || !is_valid_control_register(index)) {
        raise_ud_ctx(ctx);
        return;
    }

    ctx->control_regs[index] = value;
}

void load_segment_register(CPU_CONTEXT* ctx, int seg_index, uint16_t selector) {
    if (seg_index == SEG_CS) {
        raise_ud_ctx(ctx);
        return;
    }

    uint8_t rpl = selector & 0x03;

    if (seg_index == SEG_SS) {
        if (is_null_selector(selector)) {
            raise_gp_ctx(ctx, 0);
            return;
        }

        SegmentDescriptor desc = load_descriptor_from_table(ctx, selector);
        if (cpu_has_exception(ctx)) {
            return;
        }

        if (rpl != ctx->cpl) {
            raise_gp_ctx(ctx, selector & 0xFFFC);
            return;
        }

        if (!is_writable_data_segment(desc.type)) {
            raise_gp_ctx(ctx, selector & 0xFFFC);
            return;
        }

        if (desc.dpl != ctx->cpl) {
            raise_gp_ctx(ctx, selector & 0xFFFC);
            return;
        }

        if (!desc.present) {
            raise_ss_ctx(ctx, selector & 0xFFFC);
            return;
        }

        ctx->ss.selector = selector;
        ctx->ss.descriptor = desc;
    }
    else {
        SegmentRegister* seg = get_segment_register(ctx, seg_index);
        if (!seg) return;

        if (is_null_selector(selector)) {
            seg->selector = selector;
            seg->descriptor = {};
        }
        else {
            SegmentDescriptor desc = load_descriptor_from_table(ctx, selector);
            if (cpu_has_exception(ctx)) {
                return;
            }

            if (!is_data_segment(desc.type) && !is_readable_code_segment(desc.type)) {
                raise_gp_ctx(ctx, selector & 0xFFFC);
                return;
            }

            if (is_data_segment(desc.type) || !is_conforming_code_segment(desc.type)) {
                if (rpl > desc.dpl || ctx->cpl > desc.dpl) {
                    raise_gp_ctx(ctx, selector & 0xFFFC);
                    return;
                }
            }

            if (!desc.present) {
                raise_np_ctx(ctx, selector & 0xFFFC);
                return;
            }

            seg->selector = selector;
            seg->descriptor = desc;
        }
    }
}

inline bool cpu_memory_base_register_uses_ss(int reg) {
    return reg == REG_RSP || reg == REG_RBP || reg == REG_R13;
}

inline int cpu_default_segment_for_memory_operand(const CPU_CONTEXT* ctx, uint8_t modrm, bool has_sib, uint8_t sib, int addr_size) {
    const uint8_t mod = (modrm >> 6) & 0x03;
    const uint8_t rm = modrm & 0x07;

    if (mod == 3) {
        return SEG_DS;
    }

    if (addr_size != 16) {
        const bool long_mode = cpu_allows_rex_prefix(ctx);
        if ((rm & 0x07) == 4 && has_sib) {
            uint8_t base = sib & 0x07;
            if (ctx && ctx->rex_b && long_mode) {
                base |= 0x08;
            }
            if (!(base == 5 && mod == 0) && cpu_memory_base_register_uses_ss(base)) {
                return SEG_SS;
            }
        }
        else {
            int decoded_rm = rm;
            if (ctx && ctx->rex_b && long_mode) {
                decoded_rm |= 0x08;
            }
            if (!(mod == 0 && (rm & 0x07) == 5) &&
                cpu_memory_base_register_uses_ss(decoded_rm)) {
                return SEG_SS;
            }
        }
    }
    else if (rm == 2 || rm == 3 || ((rm == 6) && mod != 0)) {
        return SEG_SS;
    }

    return SEG_DS;
}

uint64_t get_effective_offset(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t* sib, int32_t* disp, int addr_size, int inst_size = 0) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;
    const bool long_mode = cpu_allows_rex_prefix(ctx);

    if (ctx->rex_b && long_mode) {
        rm |= 0x08;
    }

    uint64_t addr = 0;

    if (addr_size == 32 || addr_size == 64) {
        if (mod == 3) {
            return 0;
        }

        if ((rm & 0x07) == 4) {
            uint8_t sib_byte = *sib;
            uint8_t scale = (sib_byte >> 6) & 0x03;
            uint8_t raw_index = (sib_byte >> 3) & 0x07;
            uint8_t raw_base = sib_byte & 0x07;
            uint8_t index = raw_index;
            uint8_t base = raw_base;

            if (ctx->rex_x && long_mode) {
                index |= 0x08;
            }
            if (ctx->rex_b && long_mode) {
                base |= 0x08;
            }

            bool has_index = !(raw_index == 4 && !(long_mode && ctx->rex_x));
            bool no_base = (mod == 0 && raw_base == 5);

            if (no_base) {
                addr = (uint32_t)(*disp);
            }
            else {
                addr = ctx->regs[base];
            }

            if (has_index) {
                addr += ctx->regs[index] << scale;
            }
        }
        else if ((rm & 0x07) == 5 && mod == 0) {
            if (addr_size == 64) {
                addr = ctx->rip + (uint64_t)inst_size + (int64_t)(*disp);
            }
            else {
                addr = (uint32_t)(*disp);
            }
        }
        else {
            addr = ctx->regs[rm];
        }

        if (mod == 1) {
            addr += (int8_t)(*disp & 0xFF);
        }
        else if (mod == 2) {
            addr += *disp;
        }

        if (addr_size == 32) {
            addr &= 0xFFFFFFFF;
        }
    }
    else {
        if (mod == 3) {
            return 0;
        }

        switch (rm) {
        case 0: addr = (ctx->regs[REG_RBX] + ctx->regs[REG_RSI]) & 0xFFFF; break;
        case 1: addr = (ctx->regs[REG_RBX] + ctx->regs[REG_RDI]) & 0xFFFF; break;
        case 2: addr = (ctx->regs[REG_RBP] + ctx->regs[REG_RSI]) & 0xFFFF; break;
        case 3: addr = (ctx->regs[REG_RBP] + ctx->regs[REG_RDI]) & 0xFFFF; break;
        case 4: addr = ctx->regs[REG_RSI] & 0xFFFF; break;
        case 5: addr = ctx->regs[REG_RDI] & 0xFFFF; break;
        case 6:
            if (mod == 0) {
                addr = *disp & 0xFFFF;
            }
            else {
                addr = ctx->regs[REG_RBP] & 0xFFFF;
            }
            break;
        case 7: addr = ctx->regs[REG_RBX] & 0xFFFF; break;
        }

        if (mod == 1) {
            addr = (addr + (int8_t)(*disp & 0xFF)) & 0xFFFF;
        }
        else if (mod == 2) {
            addr = (addr + (*disp & 0xFFFF)) & 0xFFFF;
        }
    }

    return addr;
}

uint64_t get_effective_address(CPU_CONTEXT* ctx, uint8_t modrm, uint8_t* sib, int32_t* disp, int addr_size, int inst_size = 0) {
    const uint8_t mod = (modrm >> 6) & 0x03;
    uint64_t addr = get_effective_offset(ctx, modrm, sib, disp, addr_size, inst_size);
    if (mod == 3) {
        return addr;
    }

    const int default_segment = cpu_default_segment_for_memory_operand(ctx, modrm, sib != NULL, sib ? *sib : 0, addr_size);
    const int segment_index = cpu_effective_segment_override_or_default(ctx, default_segment);
    addr += cpu_segment_base_for_addressing(ctx, segment_index);
    cpu_validate_linear_address(ctx, addr, segment_index);
    return addr;
}

void finalize_rip_relative_address(CPU_CONTEXT* ctx, DecodedInstruction* inst, int inst_size) {
    if (!inst->has_modrm) {
        return;
    }

    uint8_t mod = (inst->modrm >> 6) & 0x03;
    if (mod == 3 || inst->address_size != 64) {
        return;
    }

    uint8_t rm = inst->modrm & 0x07;
    if (ctx->rex_b) {
        rm |= 0x08;
    }

    bool rip_relative = false;
    if ((rm & 0x07) != 4 || !inst->has_sib) {
        rip_relative = ((rm & 0x07) == 5 && mod == 0);
    }

    if (rip_relative) {
        inst->mem_address = get_effective_address(ctx, inst->modrm, &inst->sib, &inst->displacement, inst->address_size, inst_size);
    }
}

uint8_t get_reg8(CPU_CONTEXT* ctx, int reg) {
    if (ctx->rex_present) {
        return ctx->regs[reg] & 0xFF;
    }
    else {
        if (reg < 4) {
            return ctx->regs[reg] & 0xFF;
        }
        else {
            return (ctx->regs[reg - 4] >> 8) & 0xFF;
        }
    }
}

void set_reg8(CPU_CONTEXT* ctx, int reg, uint8_t value) {
    if (ctx->rex_present) {
        ctx->regs[reg] = (ctx->regs[reg] & ~0xFFULL) | value;
    }
    else {
        if (reg < 4) {
            ctx->regs[reg] = (ctx->regs[reg] & ~0xFFULL) | value;
        }
        else {
            ctx->regs[reg - 4] = (ctx->regs[reg - 4] & ~0xFF00ULL) | ((uint64_t)value << 8);
        }
    }
}

uint16_t get_reg16(CPU_CONTEXT* ctx, int reg) {
    return ctx->regs[reg] & 0xFFFF;
}

void set_reg16(CPU_CONTEXT* ctx, int reg, uint16_t value) {
    ctx->regs[reg] = (ctx->regs[reg] & ~0xFFFFULL) | value;
}

uint32_t get_reg32(CPU_CONTEXT* ctx, int reg) {
    return ctx->regs[reg] & 0xFFFFFFFF;
}

void set_reg32(CPU_CONTEXT* ctx, int reg, uint32_t value) {
    ctx->regs[reg] = value;
}

uint64_t get_reg64(CPU_CONTEXT* ctx, int reg) {
    return ctx->regs[reg];
}

void set_reg64(CPU_CONTEXT* ctx, int reg, uint64_t value) {
    ctx->regs[reg] = value;
}

XMMRegister get_xmm128(CPU_CONTEXT* ctx, int reg) {
    return ctx->xmm[reg];
}

void set_xmm128(CPU_CONTEXT* ctx, int reg, XMMRegister value) {
    ctx->xmm[reg] = value;
}

void set_xmm128_parts(CPU_CONTEXT* ctx, int reg, uint64_t low, uint64_t high) {
    ctx->xmm[reg].low = low;
    ctx->xmm[reg].high = high;
}

XMMRegister get_ymm_upper128(CPU_CONTEXT* ctx, int reg) {
    return ctx->ymm_upper[reg];
}

void set_ymm_upper128(CPU_CONTEXT* ctx, int reg, XMMRegister value) {
    ctx->ymm_upper[reg] = value;
}

void set_ymm_upper128_parts(CPU_CONTEXT* ctx, int reg, uint64_t low, uint64_t high) {
    ctx->ymm_upper[reg].low = low;
    ctx->ymm_upper[reg].high = high;
}

ZMMUpperRegister get_zmm_upper256(CPU_CONTEXT* ctx, int reg);
void set_zmm_upper256(CPU_CONTEXT* ctx, int reg, ZMMUpperRegister value);
void clear_zmm_upper256(CPU_CONTEXT* ctx, int reg);
void clear_all_zmm_upper256(CPU_CONTEXT* ctx);

void clear_ymm_upper128(CPU_CONTEXT* ctx, int reg) {
    ctx->ymm_upper[reg] = {};
    clear_zmm_upper256(ctx, reg);
}

void clear_all_ymm_upper128(CPU_CONTEXT* ctx) {
    CPUEAXH_MEMSET(ctx->ymm_upper, 0, sizeof(ctx->ymm_upper));
    clear_all_zmm_upper256(ctx);
}

ZMMUpperRegister get_zmm_upper256(CPU_CONTEXT* ctx, int reg) {
    return ctx->zmm_upper[reg];
}

void set_zmm_upper256(CPU_CONTEXT* ctx, int reg, ZMMUpperRegister value) {
    ctx->zmm_upper[reg] = value;
}

void clear_zmm_upper256(CPU_CONTEXT* ctx, int reg) {
    ctx->zmm_upper[reg] = {};
}

void clear_all_zmm_upper256(CPU_CONTEXT* ctx) {
    CPUEAXH_MEMSET(ctx->zmm_upper, 0, sizeof(ctx->zmm_upper));
}

ZMMRegister get_zmm512(CPU_CONTEXT* ctx, int reg) {
    ZMMRegister value = {};
    value.xmm0 = ctx->xmm[reg];
    value.xmm1 = ctx->ymm_upper[reg];
    value.xmm2 = ctx->zmm_upper[reg].lower;
    value.xmm3 = ctx->zmm_upper[reg].upper;
    return value;
}

void set_zmm512(CPU_CONTEXT* ctx, int reg, ZMMRegister value) {
    ctx->xmm[reg] = value.xmm0;
    ctx->ymm_upper[reg] = value.xmm1;
    ctx->zmm_upper[reg].lower = value.xmm2;
    ctx->zmm_upper[reg].upper = value.xmm3;
}

void clear_zmm_above_128(CPU_CONTEXT* ctx, int reg) {
    clear_ymm_upper128(ctx, reg);
    clear_zmm_upper256(ctx, reg);
}

void clear_zmm_above_256(CPU_CONTEXT* ctx, int reg) {
    clear_zmm_upper256(ctx, reg);
}

uint64_t get_opmask64(CPU_CONTEXT* ctx, int reg) {
    return ctx->opmask[reg & 0x07];
}

void set_opmask64(CPU_CONTEXT* ctx, int reg, uint64_t value) {
    ctx->opmask[reg & 0x07] = value;
}

uint64_t get_mm64(CPU_CONTEXT* ctx, int reg) {
    return ctx->mm[reg & 0x07];
}

void set_mm64(CPU_CONTEXT* ctx, int reg, uint64_t value) {
    ctx->mm[reg & 0x07] = value;
}

inline uint64_t cpu_load_u64_le(const uint8_t* bytes) {
    return (uint64_t)bytes[0] |
        ((uint64_t)bytes[1] << 8) |
        ((uint64_t)bytes[2] << 16) |
        ((uint64_t)bytes[3] << 24) |
        ((uint64_t)bytes[4] << 32) |
        ((uint64_t)bytes[5] << 40) |
        ((uint64_t)bytes[6] << 48) |
        ((uint64_t)bytes[7] << 56);
}

inline void cpu_store_u64_le(uint8_t* bytes, uint64_t value) {
    for (int index = 0; index < 8; ++index) {
        bytes[index] = (uint8_t)((value >> (index * 8)) & 0xFFu);
    }
}

inline uint16_t cpu_load_u16_le(const uint8_t* bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

inline uint32_t cpu_load_u32_le(const uint8_t* bytes) {
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

inline void cpu_store_u16_le(uint8_t* bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
}

inline void cpu_store_u32_le(uint8_t* bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
}

inline bool cpu_read_linear_bytes(CPU_CONTEXT* ctx, uint64_t address, uint8_t* out, size_t size) {
    if (!out || size == 0 || size > 64 || cpu_has_exception(ctx)) {
        return false;
    }

    uint8_t* contiguous = cpu_try_get_contiguous_ptr(ctx, address, size, MM_PROT_READ);
    if (contiguous) {
        CPUEAXH_MEMCPY(out, contiguous, size);
        return true;
    }
    if (cpu_has_exception(ctx)) {
        return false;
    }

    uint8_t* byte_ptrs[64] = {};
    for (size_t offset = 0; offset < size; ++offset) {
        if (cpu_resolve_memory_access(ctx, address + offset, MM_PROT_READ, &byte_ptrs[offset], address, size, 0) != MM_ACCESS_OK) {
            return false;
        }
    }
    for (size_t offset = 0; offset < size; ++offset) {
        out[offset] = *byte_ptrs[offset];
    }
    return true;
}

inline bool cpu_read_linear_bytes_large(CPU_CONTEXT* ctx, uint64_t address, uint8_t* out, size_t size) {
    if (!out || size == 0 || size > 512 || cpu_has_exception(ctx)) {
        return false;
    }

    uint8_t* contiguous = cpu_try_get_contiguous_ptr(ctx, address, size, MM_PROT_READ);
    if (contiguous) {
        CPUEAXH_MEMCPY(out, contiguous, size);
        return true;
    }
    if (cpu_has_exception(ctx)) {
        return false;
    }

    uint8_t* byte_ptrs[512] = {};
    for (size_t offset = 0; offset < size; ++offset) {
        if (cpu_resolve_memory_access(ctx, address + offset, MM_PROT_READ, &byte_ptrs[offset], address, size, 0) != MM_ACCESS_OK) {
            return false;
        }
    }
    for (size_t offset = 0; offset < size; ++offset) {
        out[offset] = *byte_ptrs[offset];
    }
    return true;
}

inline bool cpu_write_linear_bytes(CPU_CONTEXT* ctx, uint64_t address, const uint8_t* bytes, size_t size, uint64_t reported_value = 0) {
    if (!bytes || size == 0 || size > 64 || cpu_has_exception(ctx)) {
        return false;
    }

    uint8_t* contiguous = cpu_try_get_contiguous_ptr(ctx, address, size, MM_PROT_WRITE, reported_value);
    if (contiguous) {
        CPUEAXH_MEMCPY(contiguous, bytes, size);
        return true;
    }
    if (cpu_has_exception(ctx)) {
        return false;
    }

    uint8_t* byte_ptrs[64] = {};
    for (size_t offset = 0; offset < size; ++offset) {
        if (cpu_resolve_memory_access(ctx, address + offset, MM_PROT_WRITE, &byte_ptrs[offset], address, size, reported_value) != MM_ACCESS_OK) {
            return false;
        }
    }
    for (size_t offset = 0; offset < size; ++offset) {
        *byte_ptrs[offset] = bytes[offset];
    }
    return true;
}

inline bool cpu_resolve_linear_write_byte_ptrs(CPU_CONTEXT* ctx, uint64_t address, size_t size, uint8_t** byte_ptrs, uint64_t reported_value = 0) {
    if (!byte_ptrs || size == 0 || size > 512 || cpu_has_exception(ctx)) {
        return false;
    }

    for (size_t offset = 0; offset < size; ++offset) {
        if (cpu_resolve_memory_access(ctx, address + offset, MM_PROT_WRITE, &byte_ptrs[offset], address, size, reported_value) != MM_ACCESS_OK) {
            return false;
        }
    }
    return true;
}

inline void cpu_commit_linear_write_bytes(const uint8_t* bytes, size_t size, uint8_t** byte_ptrs) {
    if (!bytes || !byte_ptrs) {
        return;
    }

    for (size_t offset = 0; offset < size; ++offset) {
        *byte_ptrs[offset] = bytes[offset];
    }
}

inline bool cpu_resolve_masked_write_element_ptrs(CPU_CONTEXT* ctx, uint64_t address, const bool* element_mask,
    const uint64_t* element_values, size_t element_count, size_t element_size, uint8_t** byte_ptrs) {
    const size_t total_size = element_count * element_size;
    if (!element_mask || !byte_ptrs || element_count == 0 || element_size == 0 ||
        total_size > 64 || total_size / element_size != element_count || cpu_has_exception(ctx)) {
        return false;
    }

    for (size_t element = 0; element < element_count; ++element) {
        if (!element_mask[element]) {
            continue;
        }

        const uint64_t element_address = address + (uint64_t)(element * element_size);
        const uint64_t reported_value = element_values ? element_values[element] : 0;
        for (size_t offset = 0; offset < element_size; ++offset) {
            const size_t byte_index = element * element_size + offset;
            if (cpu_resolve_memory_access(ctx, element_address + offset, MM_PROT_WRITE, &byte_ptrs[byte_index],
                element_address, element_size, reported_value) != MM_ACCESS_OK) {
                return false;
            }
        }
    }
    return true;
}

inline void cpu_commit_masked_write_elements(const uint8_t* bytes, const bool* element_mask,
    size_t element_count, size_t element_size, uint8_t** byte_ptrs) {
    if (!bytes || !element_mask || !byte_ptrs) {
        return;
    }

    for (size_t element = 0; element < element_count; ++element) {
        if (!element_mask[element]) {
            continue;
        }

        for (size_t offset = 0; offset < element_size; ++offset) {
            const size_t byte_index = element * element_size + offset;
            *byte_ptrs[byte_index] = bytes[byte_index];
        }
    }
}

inline void cpu_notify_qword_memory_hooks(CPU_CONTEXT* ctx, uint32_t hook_type, uint64_t address, const uint8_t* bytes, size_t size) {
    if (!cpu_has_hook_type(ctx, hook_type)) {
        return;
    }
    for (size_t offset = 0; offset < size; offset += 8) {
        cpu_notify_memory_hook(ctx, hook_type, address + offset, 8, cpu_load_u64_le(bytes + offset));
    }
}

XMMRegister read_xmm_memory(CPU_CONTEXT* ctx, uint64_t address) {
    XMMRegister value = {};
    uint8_t bytes[16] = {};
    if (!cpu_read_linear_bytes(ctx, address, bytes, sizeof(bytes))) {
        return value;
    }
    value.low = cpu_load_u64_le(bytes);
    value.high = cpu_load_u64_le(bytes + 8);
    cpu_notify_qword_memory_hooks(ctx, CPUEAXH_HOOK_MEM_READ, address, bytes, sizeof(bytes));
    return value;
}

void write_xmm_memory(CPU_CONTEXT* ctx, uint64_t address, XMMRegister value) {
    uint8_t bytes[16] = {};
    cpu_store_u64_le(bytes, value.low);
    cpu_store_u64_le(bytes + 8, value.high);
    if (cpu_has_exception(ctx)) {
        return;
    }
    uint8_t* contiguous = cpu_try_get_contiguous_ptr(ctx, address, sizeof(bytes), MM_PROT_WRITE, value.low);
    if (!contiguous && cpu_has_exception(ctx)) {
        return;
    }
    cpu_notify_qword_memory_hooks(ctx, CPUEAXH_HOOK_MEM_WRITE, address, bytes, sizeof(bytes));
    if (contiguous) {
        CPUEAXH_MEMCPY(contiguous, bytes, sizeof(bytes));
        return;
    }
    cpu_write_linear_bytes(ctx, address, bytes, sizeof(bytes), value.low);
}

ZMMUpperRegister read_zmm_upper_memory(CPU_CONTEXT* ctx, uint64_t address) {
    ZMMUpperRegister value = {};
    uint8_t bytes[32] = {};
    if (!cpu_read_linear_bytes(ctx, address, bytes, sizeof(bytes))) {
        return value;
    }
    value.lower.low = cpu_load_u64_le(bytes);
    value.lower.high = cpu_load_u64_le(bytes + 8);
    value.upper.low = cpu_load_u64_le(bytes + 16);
    value.upper.high = cpu_load_u64_le(bytes + 24);
    cpu_notify_qword_memory_hooks(ctx, CPUEAXH_HOOK_MEM_READ, address, bytes, sizeof(bytes));
    return value;
}

void write_zmm_upper_memory(CPU_CONTEXT* ctx, uint64_t address, ZMMUpperRegister value) {
    uint8_t bytes[32] = {};
    cpu_store_u64_le(bytes, value.lower.low);
    cpu_store_u64_le(bytes + 8, value.lower.high);
    cpu_store_u64_le(bytes + 16, value.upper.low);
    cpu_store_u64_le(bytes + 24, value.upper.high);
    if (cpu_has_exception(ctx)) {
        return;
    }
    uint8_t* contiguous = cpu_try_get_contiguous_ptr(ctx, address, sizeof(bytes), MM_PROT_WRITE, value.lower.low);
    if (!contiguous && cpu_has_exception(ctx)) {
        return;
    }
    cpu_notify_qword_memory_hooks(ctx, CPUEAXH_HOOK_MEM_WRITE, address, bytes, sizeof(bytes));
    if (contiguous) {
        CPUEAXH_MEMCPY(contiguous, bytes, sizeof(bytes));
        return;
    }
    cpu_write_linear_bytes(ctx, address, bytes, sizeof(bytes), value.lower.low);
}

ZMMRegister read_zmm_memory(CPU_CONTEXT* ctx, uint64_t address) {
    ZMMRegister value = {};
    uint8_t bytes[64] = {};
    if (!cpu_read_linear_bytes(ctx, address, bytes, sizeof(bytes))) {
        return value;
    }
    value.xmm0.low = cpu_load_u64_le(bytes);
    value.xmm0.high = cpu_load_u64_le(bytes + 8);
    value.xmm1.low = cpu_load_u64_le(bytes + 16);
    value.xmm1.high = cpu_load_u64_le(bytes + 24);
    value.xmm2.low = cpu_load_u64_le(bytes + 32);
    value.xmm2.high = cpu_load_u64_le(bytes + 40);
    value.xmm3.low = cpu_load_u64_le(bytes + 48);
    value.xmm3.high = cpu_load_u64_le(bytes + 56);
    cpu_notify_qword_memory_hooks(ctx, CPUEAXH_HOOK_MEM_READ, address, bytes, sizeof(bytes));
    return value;
}

void write_zmm_memory(CPU_CONTEXT* ctx, uint64_t address, ZMMRegister value) {
    uint8_t bytes[64] = {};
    cpu_store_u64_le(bytes, value.xmm0.low);
    cpu_store_u64_le(bytes + 8, value.xmm0.high);
    cpu_store_u64_le(bytes + 16, value.xmm1.low);
    cpu_store_u64_le(bytes + 24, value.xmm1.high);
    cpu_store_u64_le(bytes + 32, value.xmm2.low);
    cpu_store_u64_le(bytes + 40, value.xmm2.high);
    cpu_store_u64_le(bytes + 48, value.xmm3.low);
    cpu_store_u64_le(bytes + 56, value.xmm3.high);
    if (cpu_has_exception(ctx)) {
        return;
    }
    uint8_t* contiguous = cpu_try_get_contiguous_ptr(ctx, address, sizeof(bytes), MM_PROT_WRITE, value.xmm0.low);
    if (!contiguous && cpu_has_exception(ctx)) {
        return;
    }
    cpu_notify_qword_memory_hooks(ctx, CPUEAXH_HOOK_MEM_WRITE, address, bytes, sizeof(bytes));
    if (contiguous) {
        CPUEAXH_MEMCPY(contiguous, bytes, sizeof(bytes));
        return;
    }
    cpu_write_linear_bytes(ctx, address, bytes, sizeof(bytes), value.xmm0.low);
}

// --- Stack operation helpers ---

// Determine stack address size: in 64-bit mode always 64; otherwise based on SS.B flag
int get_stack_addr_size(CPU_CONTEXT* ctx) {
    return cpu_stack_address_size(ctx);
}

// Push a 16-bit value onto the stack
void push_value16(CPU_CONTEXT* ctx, uint16_t value) {
    if (cpu_has_exception(ctx)) {
        return;
    }
    int stack_addr_size = get_stack_addr_size(ctx);
    if (stack_addr_size == 64) {
        uint64_t new_rsp = ctx->regs[REG_RSP] - 2;
        write_memory_word(ctx, new_rsp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = new_rsp;
        }
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        esp -= 2;
        write_memory_word(ctx, esp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
        }
    }
    else {
        uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFF);
        sp -= 2;
        write_memory_word(ctx, sp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | sp;
        }
    }
}

// Push a 32-bit value onto the stack
void push_value32(CPU_CONTEXT* ctx, uint32_t value) {
    if (cpu_has_exception(ctx)) {
        return;
    }
    int stack_addr_size = get_stack_addr_size(ctx);
    if (stack_addr_size == 64) {
        uint64_t new_rsp = ctx->regs[REG_RSP] - 4;
        write_memory_dword(ctx, new_rsp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = new_rsp;
        }
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        esp -= 4;
        write_memory_dword(ctx, esp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
        }
    }
    else {
        uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFF);
        sp -= 4;
        write_memory_dword(ctx, sp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | sp;
        }
    }
}

// Push a 64-bit value onto the stack
void push_value64(CPU_CONTEXT* ctx, uint64_t value) {
    if (cpu_has_exception(ctx)) {
        return;
    }
    int stack_addr_size = get_stack_addr_size(ctx);
    if (stack_addr_size == 64) {
        uint64_t new_rsp = ctx->regs[REG_RSP] - 8;
        write_memory_qword(ctx, new_rsp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = new_rsp;
        }
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        esp -= 8;
        write_memory_qword(ctx, esp, value);
        if (!cpu_has_exception(ctx)) {
            ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
        }
    }
    else {
        // 16-bit stack address doesn't support 64-bit pushes
        raise_gp_ctx(ctx, 0);
    }
}

inline void cpu_store_stack_value_le(uint8_t* bytes, size_t size, uint64_t value) {
    switch (size) {
    case 2:
        cpu_store_u16_le(bytes, (uint16_t)value);
        break;
    case 4:
        cpu_store_u32_le(bytes, (uint32_t)value);
        break;
    case 8:
        cpu_store_u64_le(bytes, value);
        break;
    default:
        break;
    }
}

inline bool cpu_resolve_stack_slot_write(CPU_CONTEXT* ctx, uint64_t address, size_t size, uint64_t value, uint8_t** byte_ptrs) {
    for (size_t offset = 0; offset < size; ++offset) {
        if (cpu_resolve_memory_access(ctx, address + offset, MM_PROT_WRITE, &byte_ptrs[offset], address, size, value) != MM_ACCESS_OK) {
            return false;
        }
    }
    return true;
}

inline void cpu_commit_stack_slot_write(uint64_t address, const uint8_t* bytes, size_t size, uint64_t value, uint8_t** byte_ptrs, CPU_CONTEXT* ctx) {
    if (cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_WRITE)) {
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_WRITE, address, size, value);
    }
    for (size_t offset = 0; offset < size; ++offset) {
        *byte_ptrs[offset] = bytes[offset];
    }
}

void push_two_stack_values(CPU_CONTEXT* ctx, uint64_t first_value, uint64_t second_value, size_t value_size) {
    if (cpu_has_exception(ctx)) {
        return;
    }
    if (value_size != 2 && value_size != 4 && value_size != 8) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    const int stack_addr_size = get_stack_addr_size(ctx);
    if (stack_addr_size == 16 && value_size == 8) {
        raise_gp_ctx(ctx, 0);
        return;
    }

    uint64_t first_address = 0;
    uint64_t second_address = 0;
    if (stack_addr_size == 64) {
        first_address = ctx->regs[REG_RSP] - value_size;
        second_address = first_address - value_size;
    }
    else if (stack_addr_size == 32) {
        const uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFFULL);
        first_address = (uint32_t)(esp - (uint32_t)value_size);
        second_address = (uint32_t)((uint32_t)first_address - (uint32_t)value_size);
    }
    else {
        const uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFFULL);
        first_address = (uint16_t)(sp - (uint16_t)value_size);
        second_address = (uint16_t)((uint16_t)first_address - (uint16_t)value_size);
    }

    uint8_t first_bytes[8] = {};
    uint8_t second_bytes[8] = {};
    uint8_t* first_ptrs[8] = {};
    uint8_t* second_ptrs[8] = {};
    cpu_store_stack_value_le(first_bytes, value_size, first_value);
    cpu_store_stack_value_le(second_bytes, value_size, second_value);

    if (!cpu_resolve_stack_slot_write(ctx, first_address, value_size, first_value, first_ptrs) ||
        !cpu_resolve_stack_slot_write(ctx, second_address, value_size, second_value, second_ptrs)) {
        return;
    }

    cpu_commit_stack_slot_write(first_address, first_bytes, value_size, first_value, first_ptrs, ctx);
    cpu_commit_stack_slot_write(second_address, second_bytes, value_size, second_value, second_ptrs, ctx);

    if (stack_addr_size == 64) {
        ctx->regs[REG_RSP] = second_address;
    }
    else if (stack_addr_size == 32) {
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | (uint32_t)second_address;
    }
    else {
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | (uint16_t)second_address;
    }
}

inline uint64_t cpu_get_stack_pointer_value(CPU_CONTEXT* ctx, int stack_addr_size) {
    if (stack_addr_size == 64) {
        return ctx->regs[REG_RSP];
    }
    if (stack_addr_size == 32) {
        return (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFFULL);
    }
    return (uint16_t)(ctx->regs[REG_RSP] & 0xFFFFULL);
}

inline uint64_t cpu_stack_pointer_add(int stack_addr_size, uint64_t value, size_t delta) {
    if (stack_addr_size == 64) {
        return value + delta;
    }
    if (stack_addr_size == 32) {
        return (uint32_t)((uint32_t)value + (uint32_t)delta);
    }
    return (uint16_t)((uint16_t)value + (uint16_t)delta);
}

inline void cpu_set_stack_pointer_value(CPU_CONTEXT* ctx, int stack_addr_size, uint64_t value) {
    if (stack_addr_size == 64) {
        ctx->regs[REG_RSP] = value;
    }
    else if (stack_addr_size == 32) {
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | (uint32_t)value;
    }
    else {
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | (uint16_t)value;
    }
}

bool peek_stack_values(CPU_CONTEXT* ctx, size_t value_size, size_t count, uint64_t* values, uint64_t* final_sp) {
    if (cpu_has_exception(ctx)) {
        return false;
    }
    if ((value_size != 2 && value_size != 4 && value_size != 8) || values == nullptr || final_sp == nullptr) {
        raise_gp_ctx(ctx, 0);
        return false;
    }

    const int stack_addr_size = get_stack_addr_size(ctx);
    if (stack_addr_size == 16 && value_size == 8) {
        raise_gp_ctx(ctx, 0);
        return false;
    }

    const uint64_t base_sp = cpu_get_stack_pointer_value(ctx, stack_addr_size);
    for (size_t index = 0; index < count; ++index) {
        const uint64_t address = cpu_stack_pointer_add(stack_addr_size, base_sp, index * value_size);
        if (value_size == 2) {
            values[index] = read_memory_word(ctx, address);
        }
        else if (value_size == 4) {
            values[index] = read_memory_dword(ctx, address);
        }
        else {
            values[index] = read_memory_qword(ctx, address);
        }
        if (cpu_has_exception(ctx)) {
            return false;
        }
    }

    *final_sp = cpu_stack_pointer_add(stack_addr_size, base_sp, count * value_size);
    return true;
}

// Pop a 16-bit value from the stack
uint16_t pop_value16(CPU_CONTEXT* ctx) {
    int stack_addr_size = get_stack_addr_size(ctx);
    uint16_t value = 0;
    if (stack_addr_size == 64) {
        value = read_memory_word(ctx, ctx->regs[REG_RSP]);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        ctx->regs[REG_RSP] += 2;
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        value = read_memory_word(ctx, esp);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        esp += 2;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
    }
    else {
        uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFF);
        value = read_memory_word(ctx, sp);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        sp += 2;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | sp;
    }
    return value;
}

// Pop a 32-bit value from the stack
uint32_t pop_value32(CPU_CONTEXT* ctx) {
    int stack_addr_size = get_stack_addr_size(ctx);
    uint32_t value = 0;
    if (stack_addr_size == 64) {
        value = read_memory_dword(ctx, ctx->regs[REG_RSP]);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        ctx->regs[REG_RSP] += 4;
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        value = read_memory_dword(ctx, esp);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        esp += 4;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
    }
    else {
        uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFF);
        value = read_memory_dword(ctx, sp);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        sp += 4;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | sp;
    }
    return value;
}

// Pop a 64-bit value from the stack
uint64_t pop_value64(CPU_CONTEXT* ctx) {
    int stack_addr_size = get_stack_addr_size(ctx);
    uint64_t value = 0;
    if (stack_addr_size == 64) {
        value = read_memory_qword(ctx, ctx->regs[REG_RSP]);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        ctx->regs[REG_RSP] += 8;
    }
    else if (stack_addr_size == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFF);
        value = read_memory_qword(ctx, esp);
        if (cpu_has_exception(ctx)) {
            return 0;
        }
        esp += 8;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
    }
    else {
        // 16-bit stack address doesn't support 64-bit pops
        raise_gp_ctx(ctx, 0);
        value = 0; // unreachable, suppress warning
    }
    return value;
}
