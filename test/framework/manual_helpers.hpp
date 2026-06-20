#pragma once
// Split from test/demo/framework.hpp: manual helper runners and low-level scenario helpers.
// This header is self-contained and may be included directly; framework.hpp is only an umbrella include.

#include "generated_specs.hpp"

namespace cpueaxh_test {

inline bool run_context_api_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_extended_context(seed);
        initial.rip = seeded(seed, 0xC00);

        err = cpueaxh_context_write(engine, &initial);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "cpueaxh_context_write failed";
            break;
        }

        cpueaxh_x86_context roundtrip{};
        err = cpueaxh_context_read(engine, &roundtrip);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "cpueaxh_context_read failed";
            break;
        }
        if (!equal_context(initial, roundtrip)) {
            failure.case_name = name;
            failure.detail = "context roundtrip mismatch";
            break;
        }

        cpueaxh_x86_ymm ymm_written = make_seeded_ymm(seed, 0xC10);
        if (!write_engine_ymm(engine, CPUEAXH_X86_REG_YMM3, ymm_written)) {
            failure.case_name = name;
            failure.detail = "write ymm3 failed";
            break;
        }
        cpueaxh_x86_ymm ymm_read{};
        if (!read_engine_ymm(engine, CPUEAXH_X86_REG_YMM3, ymm_read) ||
            !equal_xmm(ymm_read.lower, ymm_written.lower) ||
            !equal_xmm(ymm_read.upper, ymm_written.upper)) {
            failure.case_name = name;
            failure.detail = "read ymm3 mismatch";
            break;
        }

        const cpueaxh_x86_zmm zmm_written = make_seeded_zmm(seed, 0xC18);
        if (!write_engine_zmm(engine, CPUEAXH_X86_REG_ZMM3, zmm_written)) {
            failure.case_name = name;
            failure.detail = "write zmm3 failed";
            break;
        }
        cpueaxh_x86_zmm zmm_read{};
        if (!read_engine_zmm(engine, CPUEAXH_X86_REG_ZMM3, zmm_read) || !equal_zmm(zmm_read, zmm_written)) {
            failure.case_name = name;
            failure.detail = "read zmm3 mismatch";
            break;
        }
        if (!read_engine_ymm(engine, CPUEAXH_X86_REG_YMM3, ymm_read) ||
            !equal_ymm(ymm_read, zmm_written.lower)) {
            failure.case_name = name;
            failure.detail = "ymm3/zmm3 coupling mismatch";
            break;
        }

        if (!write_engine_ymm(engine, CPUEAXH_X86_REG_YMM3, ymm_written)) {
            failure.case_name = name;
            failure.detail = "rewrite ymm3 failed";
            break;
        }
        if (!read_engine_zmm(engine, CPUEAXH_X86_REG_ZMM3, zmm_read) ||
            !equal_ymm(zmm_read.lower, ymm_written) ||
            !equal_ymm(zmm_read.upper, zmm_written.upper)) {
            failure.case_name = name;
            failure.detail = "zmm3/ymm3 coupling mismatch";
            break;
        }

        cpueaxh_x86_xmm xmm_written = make_seeded_xmm(seed, 0xC20);
        if (!write_engine_xmm(engine, CPUEAXH_X86_REG_XMM3, xmm_written)) {
            failure.case_name = name;
            failure.detail = "write xmm3 failed";
            break;
        }
        cpueaxh_x86_xmm xmm_read{};
        if (!read_engine_xmm(engine, CPUEAXH_X86_REG_XMM3, xmm_read) || !equal_xmm(xmm_read, xmm_written)) {
            failure.case_name = name;
            failure.detail = "read xmm3 mismatch";
            break;
        }
        if (!read_engine_ymm(engine, CPUEAXH_X86_REG_YMM3, ymm_read) ||
            !equal_xmm(ymm_read.lower, xmm_written) ||
            !equal_xmm(ymm_read.upper, ymm_written.upper)) {
            failure.case_name = name;
            failure.detail = "ymm3/xmm3 coupling mismatch";
            break;
        }
        if (!read_engine_zmm(engine, CPUEAXH_X86_REG_ZMM3, zmm_read) ||
            !equal_xmm(zmm_read.lower.lower, xmm_written) ||
            !equal_xmm(zmm_read.lower.upper, ymm_written.upper) ||
            !equal_ymm(zmm_read.upper, zmm_written.upper)) {
            failure.case_name = name;
            failure.detail = "zmm3/xmm3 coupling mismatch";
            break;
        }

        const std::uint64_t k1_written = seeded(seed, 0xC28);
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_K1, k1_written)) {
            failure.case_name = name;
            failure.detail = "write k1 failed";
            break;
        }
        std::uint64_t k1_read = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_K1, k1_read) || k1_read != k1_written) {
            failure.case_name = name;
            failure.detail = "read k1 mismatch";
            break;
        }

        const std::uint64_t mm0_written = seeded(seed, 0xC30);
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_MM0, mm0_written)) {
            failure.case_name = name;
            failure.detail = "write mm0 failed";
            break;
        }
        std::uint64_t mm0_read = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_MM0, mm0_read) || mm0_read != mm0_written) {
            failure.case_name = name;
            failure.detail = "read mm0 mismatch";
            break;
        }

        const std::uint32_t mxcsr_written = static_cast<std::uint32_t>(0x1F80u | (seeded(seed, 0xC31) & 0x1F3Fu));
        if (!write_engine_reg32(engine, CPUEAXH_X86_REG_MXCSR, mxcsr_written)) {
            failure.case_name = name;
            failure.detail = "write mxcsr failed";
            break;
        }
        std::uint32_t mxcsr_read = 0;
        if (!read_engine_reg32(engine, CPUEAXH_X86_REG_MXCSR, mxcsr_read) || mxcsr_read != mxcsr_written) {
            failure.case_name = name;
            failure.detail = "read mxcsr mismatch";
            break;
        }

        const std::uint16_t fs_selector = static_cast<std::uint16_t>(seeded(seed, 0xC40) & 0xFFF8u);
        const std::uint64_t fs_base = seeded(seed, 0xC41);
        const std::uint32_t fs_granularity = static_cast<std::uint32_t>(seeded(seed, 0xC44) & 1u);
        if (!write_engine_reg16(engine, CPUEAXH_X86_REG_FS_SELECTOR, fs_selector) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_FS_BASE, fs_base) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_FS_GRANULARITY, fs_granularity)) {
            failure.case_name = name;
            failure.detail = "write fs failed";
            break;
        }
        std::uint16_t fs_selector_read = 0;
        std::uint64_t fs_base_read = 0;
        std::uint32_t fs_granularity_read = 0;
        if (!read_engine_reg16(engine, CPUEAXH_X86_REG_FS_SELECTOR, fs_selector_read) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_FS_BASE, fs_base_read) ||
            !read_engine_reg32(engine, CPUEAXH_X86_REG_FS_GRANULARITY, fs_granularity_read) ||
            fs_selector_read != fs_selector ||
            fs_base_read != fs_base ||
            fs_granularity_read != fs_granularity) {
            failure.case_name = name;
            failure.detail = "read fs mismatch";
            break;
        }

        const std::uint16_t gs_selector = static_cast<std::uint16_t>(seeded(seed, 0xC42) & 0xFFF8u);
        const std::uint64_t gs_base = seeded(seed, 0xC43);
        const std::uint32_t gs_db = static_cast<std::uint32_t>(seeded(seed, 0xC45) & 1u);
        const std::uint32_t gs_long_mode = static_cast<std::uint32_t>(seeded(seed, 0xC46) & 1u);
        if (!write_engine_reg16(engine, CPUEAXH_X86_REG_GS_SELECTOR, gs_selector) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_GS_BASE, gs_base) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_GS_DB, gs_db) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_GS_LONG_MODE, gs_long_mode)) {
            failure.case_name = name;
            failure.detail = "write gs failed";
            break;
        }
        std::uint32_t gs_db_read = 0;
        std::uint32_t gs_long_mode_read = 0;
        if (!read_engine_reg32(engine, CPUEAXH_X86_REG_GS_DB, gs_db_read) ||
            !read_engine_reg32(engine, CPUEAXH_X86_REG_GS_LONG_MODE, gs_long_mode_read) ||
            gs_db_read != gs_db ||
            gs_long_mode_read != gs_long_mode) {
            failure.case_name = name;
            failure.detail = "read gs mismatch";
            break;
        }

        const std::uint32_t es_limit = narrow32(seeded(seed, 0xC47));
        const std::uint32_t cs_type = static_cast<std::uint32_t>(seeded(seed, 0xC48) & 0x1Fu);
        const std::uint32_t ss_dpl = static_cast<std::uint32_t>(seeded(seed, 0xC49) & 0x3u);
        const std::uint32_t ds_present = static_cast<std::uint32_t>(seeded(seed, 0xC4A) & 1u);
        if (!write_engine_reg32(engine, CPUEAXH_X86_REG_ES_LIMIT, es_limit) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_TYPE, cs_type) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_DPL, ss_dpl) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_DS_PRESENT, ds_present)) {
            failure.case_name = name;
            failure.detail = "write segment descriptor fields failed";
            break;
        }
        std::uint32_t es_limit_read = 0;
        std::uint32_t cs_type_read = 0;
        std::uint32_t ss_dpl_read = 0;
        std::uint32_t ds_present_read = 0;
        if (!read_engine_reg32(engine, CPUEAXH_X86_REG_ES_LIMIT, es_limit_read) ||
            !read_engine_reg32(engine, CPUEAXH_X86_REG_CS_TYPE, cs_type_read) ||
            !read_engine_reg32(engine, CPUEAXH_X86_REG_SS_DPL, ss_dpl_read) ||
            !read_engine_reg32(engine, CPUEAXH_X86_REG_DS_PRESENT, ds_present_read) ||
            es_limit_read != es_limit ||
            cs_type_read != cs_type ||
            ss_dpl_read != ss_dpl ||
            ds_present_read != ds_present) {
            failure.case_name = name;
            failure.detail = "read segment descriptor fields mismatch";
            break;
        }

        const std::uint64_t gdtr_base = seeded(seed, 0xC50);
        const std::uint16_t gdtr_limit = static_cast<std::uint16_t>(seeded(seed, 0xC51) & 0xFFFFu);
        const std::uint64_t ldtr_base = seeded(seed, 0xC52);
        const std::uint16_t ldtr_limit = static_cast<std::uint16_t>(seeded(seed, 0xC53) & 0xFFFFu);
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_GDTR_BASE, gdtr_base) ||
            !write_engine_reg16(engine, CPUEAXH_X86_REG_GDTR_LIMIT, gdtr_limit) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_LDTR_BASE, ldtr_base) ||
            !write_engine_reg16(engine, CPUEAXH_X86_REG_LDTR_LIMIT, ldtr_limit)) {
            failure.case_name = name;
            failure.detail = "write descriptor tables failed";
            break;
        }

        const std::uint32_t exception_code = CPUEAXH_EXCEPTION_GP;
        const std::uint32_t exception_error = narrow32(seeded(seed, 0xC60));
        if (!write_engine_reg32(engine, CPUEAXH_X86_REG_EXCEPTION_CODE, exception_code) ||
            !write_engine_reg32(engine, CPUEAXH_X86_REG_EXCEPTION_ERROR_CODE, exception_error)) {
            failure.case_name = name;
            failure.detail = "write exception state failed";
            break;
        }

        const std::uint64_t cr1_written = seeded(seed, 0xC70);
        const std::uint64_t cr15_written = seeded(seed, 0xC71);
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_CR1, cr1_written) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_CR15, cr15_written)) {
            failure.case_name = name;
            failure.detail = "write extra control regs failed";
            break;
        }
        std::uint64_t cr1_read = 0;
        std::uint64_t cr15_read = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_CR1, cr1_read) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_CR15, cr15_read) ||
            cr1_read != cr1_written ||
            cr15_read != cr15_written) {
            failure.case_name = name;
            failure.detail = "read extra control regs mismatch";
            break;
        }

        const std::uint32_t processor_id = narrow32(seeded(seed, 0xC72));
        if (!write_engine_reg32(engine, CPUEAXH_X86_REG_PROCESSOR_ID, processor_id)) {
            failure.case_name = name;
            failure.detail = "write processor_id failed";
            break;
        }
        std::uint32_t processor_id_read = 0;
        if (!read_engine_reg32(engine, CPUEAXH_X86_REG_PROCESSOR_ID, processor_id_read) || processor_id_read != processor_id) {
            failure.case_name = name;
            failure.detail = "read processor_id mismatch";
            break;
        }

        cpueaxh_x86_context final_context{};
        err = cpueaxh_context_read(engine, &final_context);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "final context read failed";
            break;
        }
        if (!equal_xmm(final_context.xmm[3], xmm_written) ||
            !equal_xmm(final_context.ymm_upper[3], ymm_written.upper) ||
            final_context.mm[0] != mm0_written ||
            final_context.mxcsr != mxcsr_written ||
            final_context.es.descriptor.limit != es_limit ||
            final_context.cs.descriptor.type != cs_type ||
            final_context.ss.descriptor.dpl != ss_dpl ||
            final_context.ds.descriptor.present != ds_present ||
            final_context.fs.selector != fs_selector ||
            final_context.fs.descriptor.base != fs_base ||
            final_context.fs.descriptor.granularity != fs_granularity ||
            final_context.gs.selector != gs_selector ||
            final_context.gs.descriptor.base != gs_base ||
            final_context.gs.descriptor.db != gs_db ||
            final_context.gs.descriptor.long_mode != gs_long_mode ||
            final_context.gdtr_base != gdtr_base ||
            final_context.gdtr_limit != gdtr_limit ||
            final_context.ldtr_base != ldtr_base ||
            final_context.ldtr_limit != ldtr_limit ||
            final_context.control_regs[1] != cr1_written ||
            final_context.control_regs[15] != cr15_written ||
            final_context.code_exception != exception_code ||
            final_context.error_code_exception != exception_error ||
            final_context.processor_id != processor_id) {
            failure.case_name = name;
            failure.detail = "final context state mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool initialize_manual_engine(cpueaxh_engine* engine, const std::vector<std::uint8_t>& code, const cpueaxh_x86_context& initial, std::uint64_t guest_rsp, Failure& failure, const std::string& name) {
    cpueaxh_err err = cpueaxh_set_memory_mode(engine, CPUEAXH_MEMORY_MODE_GUEST);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_set_memory_mode failed";
        return false;
    }
    err = cpueaxh_mem_map(engine, kGuestCodeBase, kCodePageSize, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE | CPUEAXH_PROT_EXEC);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_mem_map code failed";
        return false;
    }
    err = cpueaxh_mem_map(engine, kGuestStackBase, kStackSize, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_mem_map stack failed";
        return false;
    }
    err = cpueaxh_mem_write(engine, kGuestCodeBase, code.data(), code.size());
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_mem_write failed";
        return false;
    }

    if (!write_engine_reg(engine, CPUEAXH_X86_REG_RAX, initial.regs[0]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RCX, initial.regs[1]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RDX, initial.regs[2]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RBX, initial.regs[3]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RSP, guest_rsp) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RBP, initial.regs[5]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RSI, initial.regs[6]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RDI, initial.regs[7]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R8, initial.regs[8]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R9, initial.regs[9]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R10, initial.regs[10]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R11, initial.regs[11]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R12, initial.regs[12]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R13, initial.regs[13]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R14, initial.regs[14]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_R15, initial.regs[15]) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_RIP, kGuestCodeBase) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_MXCSR, static_cast<std::uint32_t>(initial.mxcsr)) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, initial.rflags)) {
        failure.case_name = name;
        failure.detail = "register initialization failed";
        return false;
    }

    return true;
}

inline bool initialize_manual_cpu_context(CPU_CONTEXT& context, MEMORY_MANAGER& memory_manager, const std::vector<std::uint8_t>& code, const cpueaxh_x86_context& initial, std::uint64_t guest_rsp, Failure& failure, const std::string& name) {
    mm_init(&memory_manager);
    init_cpu_context(&context, &memory_manager);

    if (!mm_map_internal(&memory_manager, kGuestCodeBase, kCodePageSize, MM_PROT_READ | MM_PROT_WRITE | MM_PROT_EXEC)) {
        failure.case_name = name;
        failure.detail = "internal code map failed";
        return false;
    }
    if (!mm_map_internal(&memory_manager, kGuestStackBase, kStackSize, MM_PROT_READ | MM_PROT_WRITE)) {
        failure.case_name = name;
        failure.detail = "internal stack map failed";
        return false;
    }

    for (std::size_t index = 0; index < code.size(); ++index) {
        if (mm_write_byte_checked(&memory_manager, kGuestCodeBase + index, code[index]) != MM_ACCESS_OK) {
            failure.case_name = name;
            failure.detail = "internal code write failed";
            return false;
        }
    }

    for (std::size_t index = 0; index < 16; ++index) {
        context.regs[index] = initial.regs[index];
    }
    context.regs[REG_RSP] = guest_rsp;
    context.rip = kGuestCodeBase;
    context.rflags = initial.rflags;
    context.mxcsr = static_cast<std::uint32_t>(initial.mxcsr);
    return true;
}

inline bool run_manual_special_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint32_t processor_id,
    bool expect_rdpid,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        cpueaxh_set_processor_id(engine, processor_id);

        err = cpueaxh_emu_start_function(engine, kGuestCodeBase, 0, 1000);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "execution failed";
            break;
        }
        if (cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "unexpected exception";
            break;
        }

        std::array<std::uint64_t, 16> regs{};
        for (std::size_t index = 0; index < regs.size(); ++index) {
            if (!read_engine_reg(engine, static_cast<int>(index), regs[index])) {
                failure.case_name = name;
                failure.detail = "register read failed";
                goto cleanup;
            }
        }
        std::uint64_t flags = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, flags)) {
            failure.case_name = name;
            failure.detail = "flags read failed";
            break;
        }

        for (std::size_t index = 1; index < regs.size(); ++index) {
            if (index == static_cast<std::size_t>(Reg::RSP)) {
                continue;
            }
            if (regs[index] != initial.regs[index]) {
                failure.case_name = name;
                failure.detail = std::string(reg_name(static_cast<Reg>(index))) + " changed unexpectedly";
                goto cleanup;
            }
        }
        if ((flags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags changed unexpectedly";
            break;
        }
        if (expect_rdpid) {
            if (regs[0] != processor_id) {
                failure.case_name = name;
                failure.detail = "rdpid result mismatch";
                break;
            }
        }
        else if (regs[0] != initial.regs[0]) {
            failure.case_name = name;
            failure.detail = "rax changed unexpectedly";
            break;
        }

        ok = true;
    } while (false);

cleanup:
    cpueaxh_close(engine);
    return ok;
}

inline std::uint64_t manual_bmi2_rotr64(std::uint64_t value, unsigned int count) {
    const unsigned int amount = count & 0x3Fu;
    if (amount == 0) {
        return value;
    }
    return (value >> amount) | (value << (64u - amount));
}

inline std::uint64_t manual_bmi2_sar64(std::uint64_t value, unsigned int count) {
    const unsigned int amount = count & 0x3Fu;
    if (amount == 0) {
        return value;
    }
    std::uint64_t result = value >> amount;
    if ((value & 0x8000000000000000ull) != 0) {
        result |= (~0ull << (64u - amount));
    }
    return result;
}

inline bool run_manual_bmi2_shift_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t source_value,
    bool has_control,
    std::uint64_t control_value,
    std::uint64_t expected_dest,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t initial_dest = seeded(seed, 0xB121);
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_RAX, initial_dest) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_RBX, source_value)) {
            failure.case_name = name;
            failure.detail = "source/destination initialization failed";
            break;
        }
        if (has_control && !write_engine_reg(engine, CPUEAXH_X86_REG_RCX, control_value)) {
            failure.case_name = name;
            failure.detail = "control initialization failed";
            break;
        }

        err = cpueaxh_emu_start_function(engine, kGuestCodeBase, 0, 1000);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "execution failed";
            break;
        }
        if (cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "unexpected exception";
            break;
        }

        std::uint64_t actual_dest = 0;
        std::uint64_t actual_source = 0;
        std::uint64_t actual_control = 0;
        std::uint64_t actual_flags = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_RAX, actual_dest) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RBX, actual_source) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RCX, actual_control) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, actual_flags)) {
            failure.case_name = name;
            failure.detail = "readback failed";
            break;
        }
        if (actual_dest != expected_dest) {
            failure.case_name = name;
            failure.detail = "destination result mismatch";
            break;
        }
        if (actual_source != source_value) {
            failure.case_name = name;
            failure.detail = "source register changed unexpectedly";
            break;
        }
        if (has_control && actual_control != control_value) {
            failure.case_name = name;
            failure.detail = "control register changed unexpectedly";
            break;
        }
        if ((actual_flags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags changed unexpectedly";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_rdpmc_public_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t cr4_value,
    std::uint8_t cpl,
    bool expect_exception,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t initial_rax = 0xAAAAAAAA55555555ull ^ seeded(seed, 0xD10);
        const std::uint64_t initial_rdx = 0xCCCCCCCC33333333ull ^ seeded(seed, 0xD11);
        const std::uint64_t initial_rcx = 0x0000000000000001ull | (seeded(seed, 0xD12) & 0xFFFFFFFFull);
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_RAX, initial_rax) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_RDX, initial_rdx) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_RCX, initial_rcx) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_CR0, 0x80000011ull) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_CR4, cr4_value) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_CPL, cpl)) {
            failure.case_name = name;
            failure.detail = "rdpmc register initialization failed";
            break;
        }

        if (expect_exception) {
            err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
            if (err != CPUEAXH_ERR_EXCEPTION) {
                failure.case_name = name;
                failure.detail = "expected CPUEAXH_ERR_EXCEPTION";
                break;
            }
            if (cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_GP) {
                failure.case_name = name;
                failure.detail = "unexpected exception code";
                break;
            }
        }
        else {
            err = cpueaxh_emu_start_function(engine, kGuestCodeBase, 0, 1000);
            if (err != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "execution failed";
                break;
            }
            if (cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
                failure.case_name = name;
                failure.detail = "unexpected exception";
                break;
            }
        }

        std::uint64_t actual_rax = 0;
        std::uint64_t actual_rdx = 0;
        std::uint64_t actual_rcx = 0;
        std::uint64_t actual_flags = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_RAX, actual_rax) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RDX, actual_rdx) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RCX, actual_rcx) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, actual_flags)) {
            failure.case_name = name;
            failure.detail = "rdpmc readback failed";
            break;
        }
        if (expect_exception) {
            if (actual_rax != initial_rax || actual_rdx != initial_rdx) {
                failure.case_name = name;
                failure.detail = "rdpmc exception path changed output registers";
                break;
            }
        }
        else if (actual_rax != 0 || actual_rdx != 0) {
            failure.case_name = name;
            failure.detail = "rdpmc deterministic zero result mismatch";
            break;
        }
        if (actual_rcx != initial_rcx) {
            failure.case_name = name;
            failure.detail = "rdpmc changed rcx unexpectedly";
            break;
        }
        if ((actual_flags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "rdpmc changed flags unexpectedly";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_rdpmc_internal_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t cr4_value,
    std::uint8_t cpl,
    bool expect_exception,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t initial_rax = 0x1111111122222222ull ^ seeded(seed, 0xD20);
        const std::uint64_t initial_rdx = 0x3333333344444444ull ^ seeded(seed, 0xD21);
        const std::uint64_t initial_rcx = 0x0000000000000002ull | (seeded(seed, 0xD22) & 0xFFFFFFFFull);
        context.regs[REG_RAX] = initial_rax;
        context.regs[REG_RDX] = initial_rdx;
        context.regs[REG_RCX] = initial_rcx;
        context.control_regs[REG_CR0] = 0x80000011ull;
        context.control_regs[REG_CR4] = cr4_value;
        context.cpl = cpl;

        const int status = cpu_step(&context);
        if (expect_exception) {
            if (status != kCpuStepException) {
                failure.case_name = name;
                failure.detail = "expected CPU_STEP_EXCEPTION";
                break;
            }
            if (context.exception.code != CPUEAXH_EXCEPTION_GP) {
                failure.case_name = name;
                failure.detail = "unexpected exception code";
                break;
            }
            if (context.regs[REG_RAX] != initial_rax || context.regs[REG_RDX] != initial_rdx) {
                failure.case_name = name;
                failure.detail = "rdpmc internal exception changed output registers";
                break;
            }
        }
        else {
            if (status != kCpuStepOk || cpu_has_exception(&context)) {
                failure.case_name = name;
                failure.detail = "expected successful cpu_step";
                break;
            }
            if (context.regs[REG_RAX] != 0 || context.regs[REG_RDX] != 0) {
                failure.case_name = name;
                failure.detail = "rdpmc internal deterministic zero mismatch";
                break;
            }
            if (context.rip != kGuestCodeBase + static_cast<std::uint64_t>(code.size())) {
                failure.case_name = name;
                failure.detail = "rdpmc internal rip mismatch";
                break;
            }
        }
        if (context.regs[REG_RCX] != initial_rcx) {
            failure.case_name = name;
            failure.detail = "rdpmc internal changed rcx unexpectedly";
            break;
        }
        if ((context.rflags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "rdpmc internal changed flags unexpectedly";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_rotate_carry_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int target_reg,
    int count_reg,
    std::uint64_t target_value,
    std::uint64_t count_value,
    std::uint64_t initial_flags,
    std::uint64_t expected_target,
    std::uint64_t expected_flags,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        initial.regs[static_cast<std::size_t>(target_reg)] = target_value;
        if (count_reg >= 0) {
            initial.regs[static_cast<std::size_t>(count_reg)] = count_value;
        }
        initial.rflags = initial_flags;

        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        std::uint64_t actual_target = 0;
        std::uint64_t actual_flags = 0;
        if (!read_engine_reg(engine, target_reg, actual_target) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, actual_flags)) {
            failure.case_name = name;
            failure.detail = "readback failed";
            break;
        }

        if (actual_target != expected_target) {
            failure.case_name = name;
            failure.detail = "target result mismatch";
            break;
        }
        if ((actual_flags & kStatusMask) != (expected_flags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_double_shift_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int target_reg,
    int source_reg,
    int count_reg,
    std::uint64_t target_value,
    std::uint64_t source_value,
    std::uint64_t count_value,
    std::uint64_t initial_flags,
    std::uint64_t expected_target,
    std::uint64_t expected_flags,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        initial.regs[static_cast<std::size_t>(target_reg)] = target_value;
        initial.regs[static_cast<std::size_t>(source_reg)] = source_value;
        if (count_reg >= 0) {
            initial.regs[static_cast<std::size_t>(count_reg)] = count_value;
        }
        initial.rflags = initial_flags;

        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        std::uint64_t actual_target = 0;
        std::uint64_t actual_source = 0;
        std::uint64_t actual_flags = 0;
        if (!read_engine_reg(engine, target_reg, actual_target) ||
            !read_engine_reg(engine, source_reg, actual_source) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, actual_flags)) {
            failure.case_name = name;
            failure.detail = "readback failed";
            break;
        }

        if (actual_target != expected_target) {
            failure.case_name = name;
            failure.detail = "target result mismatch";
            break;
        }
        if (actual_source != source_value) {
            failure.case_name = name;
            failure.detail = "source changed unexpectedly";
            break;
        }
        if ((actual_flags & kStatusMask) != (expected_flags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_store_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint32_t initial_mxcsr,
    std::uint64_t store_offset,
    std::uint32_t expected_size,
    std::uint64_t expected_value,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        initial.mxcsr = initial_mxcsr;
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        err = cpueaxh_emu_start_function(engine, kGuestCodeBase, 0, 1000);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "execution failed";
            break;
        }
        if (cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "unexpected exception";
            break;
        }

        std::uint8_t buffer[8] = {};
        err = cpueaxh_mem_read(engine, guest_rsp + store_offset, buffer, expected_size);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "read stored value failed";
            break;
        }

        std::uint64_t actual_value = 0;
        for (std::uint32_t index = 0; index < expected_size; ++index) {
            actual_value |= static_cast<std::uint64_t>(buffer[index]) << (index * 8);
        }
        if (actual_value != expected_value) {
            failure.case_name = name;
            failure.detail = std::string("stored value mismatch expected=") + hex64(expected_value) + " actual=" + hex64(actual_value);
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool read_internal_word(MEMORY_MANAGER& memory_manager, std::uint64_t address, std::uint16_t& value) {
    std::uint8_t low = 0;
    std::uint8_t high = 0;
    if (mm_read_byte_checked(&memory_manager, address, &low, MM_PROT_READ) != MM_ACCESS_OK ||
        mm_read_byte_checked(&memory_manager, address + 1, &high, MM_PROT_READ) != MM_ACCESS_OK) {
        return false;
    }

    value = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
    return true;
}

inline bool write_internal_word(MEMORY_MANAGER& memory_manager, std::uint64_t address, std::uint16_t value) {
    const std::uint8_t low = static_cast<std::uint8_t>(value & 0xFFu);
    const std::uint8_t high = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    return mm_write_byte_checked(&memory_manager, address, low) == MM_ACCESS_OK &&
        mm_write_byte_checked(&memory_manager, address + 1, high) == MM_ACCESS_OK;
}

inline bool read_internal_dword(MEMORY_MANAGER& memory_manager, std::uint64_t address, std::uint32_t& value) {
    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < 4; ++index) {
        std::uint8_t byte = 0;
        if (mm_read_byte_checked(&memory_manager, address + index, &byte, MM_PROT_READ) != MM_ACCESS_OK) {
            return false;
        }
        result |= static_cast<std::uint32_t>(byte) << (index * 8);
    }
    value = result;
    return true;
}

inline bool write_internal_dword(MEMORY_MANAGER& memory_manager, std::uint64_t address, std::uint32_t value) {
    for (std::uint32_t index = 0; index < 4; ++index) {
        const std::uint8_t byte = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFu);
        if (mm_write_byte_checked(&memory_manager, address + index, byte) != MM_ACCESS_OK) {
            return false;
        }
    }
    return true;
}

inline std::uint64_t encode_segment_descriptor(std::uint32_t base, std::uint32_t limit, std::uint8_t type, std::uint8_t dpl, bool present, bool granularity, bool db, bool long_mode) {
    std::uint64_t descriptor = 0;
    descriptor |= static_cast<std::uint64_t>(limit & 0xFFFFu);
    descriptor |= static_cast<std::uint64_t>(base & 0xFFFFu) << 16;
    descriptor |= static_cast<std::uint64_t>((base >> 16) & 0xFFu) << 32;

    std::uint8_t access = static_cast<std::uint8_t>(0x10u | (type & 0x0Fu));
    access |= static_cast<std::uint8_t>((dpl & 0x03u) << 5);
    if (present) {
        access |= 0x80u;
    }
    descriptor |= static_cast<std::uint64_t>(access) << 40;

    std::uint8_t flags = static_cast<std::uint8_t>((limit >> 16) & 0x0Fu);
    if (long_mode) {
        flags |= 0x20u;
    }
    if (db) {
        flags |= 0x40u;
    }
    if (granularity) {
        flags |= 0x80u;
    }
    descriptor |= static_cast<std::uint64_t>(flags) << 48;
    descriptor |= static_cast<std::uint64_t>((base >> 24) & 0xFFu) << 56;
    return descriptor;
}

inline bool write_engine_qword(cpueaxh_engine* engine, std::uint64_t address, std::uint64_t value) {
    return cpueaxh_mem_write(engine, address, &value, sizeof(value)) == CPUEAXH_ERR_OK;
}

inline bool write_engine_dword(cpueaxh_engine* engine, std::uint64_t address, std::uint32_t value) {
    return cpueaxh_mem_write(engine, address, &value, sizeof(value)) == CPUEAXH_ERR_OK;
}

inline bool configure_compat32_segments(cpueaxh_engine* engine, Failure& failure, const std::string& name) {
    const std::uint64_t descriptors[] = {
        0,
        encode_segment_descriptor(0, 0x000FFFFFu, 0x0Bu, 0, true, true, false, true),
        encode_segment_descriptor(0, 0x000FFFFFu, 0x0Bu, 0, true, true, true, false),
        encode_segment_descriptor(0, 0x000FFFFFu, 0x03u, 0, true, true, true, false)
    };

    if (cpueaxh_mem_map(engine, kGuestGdtBase, kCodePageSize, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "gdt map failed";
        return false;
    }
    if (cpueaxh_mem_write(engine, kGuestGdtBase, descriptors, sizeof(descriptors)) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "gdt write failed";
        return false;
    }

    if (!write_engine_reg(engine, CPUEAXH_X86_REG_GDTR_BASE, kGuestGdtBase) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_GDTR_LIMIT, static_cast<std::uint16_t>(sizeof(descriptors) - 1)) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_CS_SELECTOR, 0x10u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_TYPE, 0x0Bu) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_DPL, 0u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_PRESENT, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_GRANULARITY, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_DB, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_LONG_MODE, 0u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_SS_SELECTOR, 0x18u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_TYPE, 0x03u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_DPL, 0u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_PRESENT, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_GRANULARITY, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_DB, 1u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_DS_SELECTOR, 0x18u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_ES_SELECTOR, 0x18u)) {
        failure.case_name = name;
        failure.detail = "compat32 segment setup failed";
        return false;
    }

    return true;
}

inline bool configure_compat32_user_segments(cpueaxh_engine* engine, Failure& failure, const std::string& name) {
    const std::uint64_t descriptors[] = {
        0,
        0,
        0,
        0,
        encode_segment_descriptor(0, 0x000FFFFFu, 0x0Bu, 3, true, true, true, false),
        encode_segment_descriptor(0, 0x000FFFFFu, 0x03u, 3, true, true, true, false),
        encode_segment_descriptor(0, 0x000FFFFFu, 0x0Bu, 3, true, true, false, true)
    };

    if (cpueaxh_mem_map(engine, kGuestGdtBase, kCodePageSize, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "user gdt map failed";
        return false;
    }
    if (cpueaxh_mem_write(engine, kGuestGdtBase, descriptors, sizeof(descriptors)) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "user gdt write failed";
        return false;
    }

    if (!write_engine_reg(engine, CPUEAXH_X86_REG_GDTR_BASE, kGuestGdtBase) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_GDTR_LIMIT, static_cast<std::uint16_t>(sizeof(descriptors) - 1)) ||
        !write_engine_reg(engine, CPUEAXH_X86_REG_CPL, 3u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_CS_SELECTOR, 0x23u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_TYPE, 0x0Bu) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_DPL, 3u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_PRESENT, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_GRANULARITY, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_DB, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_CS_LONG_MODE, 0u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_SS_SELECTOR, 0x2Bu) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_TYPE, 0x03u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_DPL, 3u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_PRESENT, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_GRANULARITY, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_SS_DB, 1u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_DS_SELECTOR, 0x2Bu) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_DS_TYPE, 0x03u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_DS_DPL, 3u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_DS_PRESENT, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_DS_GRANULARITY, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_DS_DB, 1u) ||
        !write_engine_reg16(engine, CPUEAXH_X86_REG_ES_SELECTOR, 0x2Bu) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_ES_TYPE, 0x03u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_ES_DPL, 3u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_ES_PRESENT, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_ES_GRANULARITY, 1u) ||
        !write_engine_reg32(engine, CPUEAXH_X86_REG_ES_DB, 1u)) {
        failure.case_name = name;
        failure.detail = "compat32 user segment setup failed";
        return false;
    }

    return true;
}

inline bool run_compat32_near_ret_case(const std::string& name, std::uint64_t seed, bool adjust_stack, Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    if (cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_COMPAT32, &engine) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open compat32 failed";
        return false;
    }

    const std::vector<std::uint8_t> code = adjust_stack
        ? std::vector<std::uint8_t>{ 0xC2, 0x04, 0x00 }
        : std::vector<std::uint8_t>{ 0xC3 };
    const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
    const std::uint32_t return_target = static_cast<std::uint32_t>(kGuestCodeBase + 0x40u);
    const std::uint32_t stack_tail = static_cast<std::uint32_t>(seeded(seed, 0xE121));

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!configure_compat32_segments(engine, failure, name)) {
            break;
        }
        if (!write_engine_dword(engine, guest_rsp, return_target) ||
            (adjust_stack && !write_engine_dword(engine, guest_rsp + 4, stack_tail))) {
            failure.case_name = name;
            failure.detail = "stack seed failed";
            break;
        }

        // NOTE: cannot use cpueaxh_emu_start_function here because it overwrites
        // [RSP] with a magic return address, which would clobber the seeded
        // return target this test wants RET to consume.
        const cpueaxh_err err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK || cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "near ret execution failed";
            break;
        }

        std::uint64_t rip = 0;
        std::uint64_t rsp = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_RIP, rip) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RSP, rsp)) {
            failure.case_name = name;
            failure.detail = "near ret readback failed";
            break;
        }

        const std::uint64_t expected_rsp = guest_rsp + 4u + (adjust_stack ? 4u : 0u);
        if (rip != return_target || (rsp & 0xFFFFFFFFull) != expected_rsp) {
            failure.case_name = name;
            failure.detail = "near ret state mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_compat32_retf_to_64_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    if (cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_COMPAT32, &engine) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open compat32 failed";
        return false;
    }

    constexpr std::uint64_t kRbxTarget = 0x1122334455667788ull;
    constexpr std::size_t kLongTargetOffset = 0x80;
    std::vector<std::uint8_t> code(kLongTargetOffset + 10, 0x90);
    code[0] = 0xB8;
    code[1] = 0x44;
    code[2] = 0x33;
    code[3] = 0x22;
    code[4] = 0x11;
    code[5] = 0xCB;
    code[kLongTargetOffset + 0] = 0x48;
    code[kLongTargetOffset + 1] = 0xBB;
    for (std::size_t index = 0; index < 8; ++index) {
        code[kLongTargetOffset + 2 + index] = static_cast<std::uint8_t>((kRbxTarget >> (index * 8)) & 0xFFu);
    }

    const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
    const std::uint32_t target_offset = static_cast<std::uint32_t>(kGuestCodeBase + kLongTargetOffset);

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!configure_compat32_segments(engine, failure, name)) {
            break;
        }
        if (!write_engine_dword(engine, guest_rsp, target_offset) ||
            !write_engine_dword(engine, guest_rsp + 4, 0x08u)) {
            failure.case_name = name;
            failure.detail = "retf stack seed failed";
            break;
        }

        // NOTE: cannot use cpueaxh_emu_start_function here because it overwrites
        // [RSP] with a magic return address, which would clobber the seeded
        // far-return offset/selector pair this test wants RETF to consume.
        const cpueaxh_err err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 3);
        if (err != CPUEAXH_ERR_OK || cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "retf execution failed";
            break;
        }

        cpueaxh_x86_context final_context{};
        if (cpueaxh_context_read(engine, &final_context) != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "retf context read failed";
            break;
        }

        if (final_context.regs[static_cast<std::size_t>(Reg::RAX)] != 0x11223344ull ||
            final_context.regs[static_cast<std::size_t>(Reg::RBX)] != kRbxTarget ||
            final_context.cs.selector != 0x08u ||
            final_context.cs.descriptor.long_mode != 1u ||
            final_context.cs.descriptor.db != 0u ||
            final_context.rip != (kGuestCodeBase + kLongTargetOffset + 10) ||
            (final_context.regs[static_cast<std::size_t>(Reg::RSP)] & 0xFFFFFFFFull) != (guest_rsp + 8u)) {
            failure.case_name = name;
            failure.detail = "retf compat32->64 state mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_compat32_far_jmp_33_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    if (cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_COMPAT32, &engine) != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open compat32 failed";
        return false;
    }

    constexpr std::uint64_t kRbxTarget = 0x8877665544332211ull;
    constexpr std::size_t kLongTargetOffset = 0xA0;
    std::vector<std::uint8_t> code(kLongTargetOffset + 10, 0x90);
    code[0] = 0xEA;
    code[1] = static_cast<std::uint8_t>(kLongTargetOffset & 0xFFu);
    code[2] = static_cast<std::uint8_t>((kLongTargetOffset >> 8) & 0xFFu);
    code[3] = 0x10;
    code[4] = 0x00;
    code[5] = 0x33;
    code[6] = 0x00;
    code[kLongTargetOffset + 0] = 0x48;
    code[kLongTargetOffset + 1] = 0xBB;
    for (std::size_t index = 0; index < 8; ++index) {
        code[kLongTargetOffset + 2 + index] = static_cast<std::uint8_t>((kRbxTarget >> (index * 8)) & 0xFFu);
    }

    const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!configure_compat32_user_segments(engine, failure, name)) {
            break;
        }

        const cpueaxh_err err = cpueaxh_emu_start_function(engine, kGuestCodeBase, 0, 2);
        if (err != CPUEAXH_ERR_OK || cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "far jmp 0x33 execution failed";
            break;
        }

        cpueaxh_x86_context final_context{};
        if (cpueaxh_context_read(engine, &final_context) != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "far jmp 0x33 context read failed";
            break;
        }

        if (final_context.regs[static_cast<std::size_t>(Reg::RBX)] != kRbxTarget ||
            final_context.cs.selector != 0x33u ||
            final_context.cs.descriptor.long_mode != 1u ||
            final_context.cs.descriptor.db != 0u ||
            final_context.cpl != 3u ||
            final_context.rip != (kGuestCodeBase + kLongTargetOffset + 10) ||
            final_context.regs[static_cast<std::size_t>(Reg::RSP)] != guest_rsp) {
            failure.case_name = name;
            failure.detail = "far jmp 0x33 state mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_x87_status_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint16_t status_word,
    bool store_to_ax,
    std::uint64_t store_offset,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        context.x87_status_word = status_word;

        const int step = cpu_step(&context);
        if (step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "status store step failed";
            break;
        }

        if ((context.rflags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags changed unexpectedly";
            break;
        }

        if (store_to_ax) {
            const std::uint64_t expected_rax = (initial.regs[0] & ~0xFFFFull) | status_word;
            if (context.regs[REG_RAX] != expected_rax) {
                failure.case_name = name;
                failure.detail = "rax status store mismatch";
                break;
            }
        }
        else {
            if (context.regs[REG_RAX] != initial.regs[0]) {
                failure.case_name = name;
                failure.detail = "rax changed unexpectedly";
                break;
            }

            std::uint16_t stored = 0;
            if (!read_internal_word(memory_manager, guest_rsp + store_offset, stored)) {
                failure.case_name = name;
                failure.detail = "status readback failed";
                break;
            }
            if (stored != status_word) {
                failure.case_name = name;
                failure.detail = "stored status mismatch";
                break;
            }
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_reset_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        context.x87_control_word = static_cast<std::uint16_t>(0x1100u | (seed & 0x00FFu));
        context.x87_status_word = static_cast<std::uint16_t>(0x2200u | ((seed >> 8) & 0x00FFu));
        context.x87_tag_word = static_cast<std::uint16_t>(0x3300u | ((seed >> 16) & 0x00FFu));
        context.x87_last_opcode = static_cast<std::uint16_t>(0x4400u | ((seed >> 24) & 0x00FFu));
        context.x87_instruction_pointer = 0x1122334455667788ull ^ seed;
        context.x87_data_pointer = 0x8877665544332211ull ^ (seed << 1);

        for (int index = 0; index < 3; ++index) {
            const int step = cpu_step(&context);
            if (step != kCpuStepOk || cpu_has_exception(&context)) {
                failure.case_name = name;
                failure.detail = "x87 reset sequence failed";
                goto cleanup;
            }
        }

        if (context.x87_control_word != kInitialX87ControlWord ||
            context.x87_status_word != kInitialX87StatusWord ||
            context.x87_tag_word != 0xFFFFu ||
            context.x87_last_opcode != 0 ||
            context.x87_instruction_pointer != 0 ||
            context.x87_data_pointer != 0) {
            failure.case_name = name;
            failure.detail = "x87 state reset mismatch";
            break;
        }

        std::uint16_t stored_control = 0;
        std::uint16_t stored_status = 0;
        if (!read_internal_word(memory_manager, guest_rsp + 0x40, stored_control) ||
            !read_internal_word(memory_manager, guest_rsp + 0x42, stored_status)) {
            failure.case_name = name;
            failure.detail = "x87 reset readback failed";
            break;
        }
        if (stored_control != kInitialX87ControlWord || stored_status != kInitialX87StatusWord) {
            failure.case_name = name;
            failure.detail = "x87 reset store mismatch";
            break;
        }

        ok = true;
    } while (false);

cleanup:
    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_emms_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint16_t initial_control = static_cast<std::uint16_t>(0x5100u | (seed & 0x00FFu));
        const std::uint16_t initial_status = static_cast<std::uint16_t>(0x2200u | ((seed >> 8) & 0x00FFu));
        const std::uint16_t initial_tag = static_cast<std::uint16_t>(seeded(seed, 0xE0) & 0x7FFFu);
        const std::uint16_t initial_opcode = static_cast<std::uint16_t>(0x4400u | ((seed >> 24) & 0x00FFu));
        const std::uint64_t initial_ip = 0x1122334455667788ull ^ seed;
        const std::uint64_t initial_dp = 0x8877665544332211ull ^ (seed << 1);
        context.x87_control_word = initial_control;
        context.x87_status_word = initial_status;
        context.x87_tag_word = initial_tag;
        context.x87_last_opcode = initial_opcode;
        context.x87_instruction_pointer = initial_ip;
        context.x87_data_pointer = initial_dp;
        context.x87_pending_exception = false;

        const int step = cpu_step(&context);
        if (step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "emms execution failed";
            break;
        }
        if (context.rip != kGuestCodeBase + static_cast<std::uint64_t>(code.size())) {
            failure.case_name = name;
            failure.detail = "emms rip advance mismatch";
            break;
        }
        for (std::size_t index = 0; index < 16; ++index) {
            if (index == static_cast<std::size_t>(REG_RSP)) {
                continue;
            }
            if (context.regs[index] != initial.regs[index]) {
                failure.case_name = name;
                failure.detail = "emms changed scalar register state";
                goto cleanup;
            }
        }
        if ((context.rflags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "emms changed flags";
            break;
        }
        if (context.x87_tag_word != 0xFFFFu) {
            failure.case_name = name;
            failure.detail = "emms did not empty x87 tag word";
            break;
        }
        if (context.x87_control_word != initial_control ||
            context.x87_status_word != initial_status ||
            context.x87_last_opcode != initial_opcode ||
            context.x87_instruction_pointer != initial_ip ||
            context.x87_data_pointer != initial_dp ||
            context.x87_pending_exception) {
            failure.case_name = name;
            failure.detail = "emms changed non-tag x87 state";
            break;
        }

        ok = true;
    } while (false);

cleanup:
    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_fldcw_load_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint16_t source_control_word,
    std::uint16_t initial_status_word,
    bool expected_pending_after_load,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_internal_word(memory_manager, guest_rsp + 0x40, source_control_word)) {
            failure.case_name = name;
            failure.detail = "source control write failed";
            break;
        }

        context.x87_control_word = kInitialX87ControlWord;
        context.x87_status_word = initial_status_word;
        context.x87_pending_exception = false;

        const int load_step = cpu_step(&context);
        if (load_step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "fldcw step failed";
            break;
        }
        if (context.x87_control_word != source_control_word) {
            failure.case_name = name;
            failure.detail = "control word load mismatch";
            break;
        }
        if (context.x87_pending_exception != expected_pending_after_load) {
            failure.case_name = name;
            failure.detail = "pending exception state mismatch";
            break;
        }

        const int store_step = cpu_step(&context);
        if (store_step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "follow-up fnstcw step failed";
            break;
        }

        std::uint16_t stored = 0;
        if (!read_internal_word(memory_manager, guest_rsp + 0x42, stored)) {
            failure.case_name = name;
            failure.detail = "control word readback failed";
            break;
        }
        if (stored != source_control_word) {
            failure.case_name = name;
            failure.detail = "stored control word mismatch";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_fldcw_unmask_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint16_t source_control_word,
    std::uint16_t initial_status_word,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_internal_word(memory_manager, guest_rsp + 0x40, source_control_word)) {
            failure.case_name = name;
            failure.detail = "source control write failed";
            break;
        }

        context.x87_control_word = kInitialX87ControlWord;
        context.x87_status_word = initial_status_word;
        context.x87_pending_exception = false;

        const int load_step = cpu_step(&context);
        if (load_step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "fldcw step failed";
            break;
        }
        if (context.x87_control_word != source_control_word || !context.x87_pending_exception) {
            failure.case_name = name;
            failure.detail = "fldcw did not arm pending exception";
            break;
        }

        const int wait_step = cpu_step(&context);
        if (wait_step != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_MF) {
            failure.case_name = name;
            failure.detail = "expected delayed x87 #MF";
            break;
        }

        std::uint16_t stored = 0;
        if (!read_internal_word(memory_manager, guest_rsp + 0x42, stored)) {
            failure.case_name = name;
            failure.detail = "control word readback failed";
            break;
        }
        if (stored != 0) {
            failure.case_name = name;
            failure.detail = "waiting instruction executed despite #MF";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_env_store_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint16_t control_word,
    std::uint16_t status_word,
    std::uint16_t tag_word,
    std::uint16_t last_opcode,
    std::uint64_t instruction_pointer,
    std::uint64_t data_pointer,
    bool pending_exception,
    bool waiting_form,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        context.x87_control_word = control_word;
        context.x87_status_word = status_word;
        context.x87_tag_word = tag_word;
        context.x87_last_opcode = last_opcode;
        context.x87_instruction_pointer = instruction_pointer;
        context.x87_data_pointer = data_pointer;
        context.x87_pending_exception = pending_exception;

        const int step = cpu_step(&context);
        if (waiting_form && pending_exception) {
            if (step != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_MF) {
                failure.case_name = name;
                failure.detail = "expected x87 #MF";
                break;
            }
            ok = true;
            break;
        }

        if (step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = std::string("fstenv/fnstenv step failed status=") + std::to_string(step) +
                " exc=" + std::to_string(context.exception.code);
            break;
        }

        std::uint32_t stored_control = 0;
        std::uint32_t stored_status = 0;
        std::uint32_t stored_tag = 0;
        std::uint32_t stored_ip = 0;
        std::uint32_t stored_opcode = 0;
        std::uint32_t stored_dp = 0;
        std::uint32_t stored_reserved = 0;
        if (!read_internal_dword(memory_manager, guest_rsp + 0x40, stored_control) ||
            !read_internal_dword(memory_manager, guest_rsp + 0x44, stored_status) ||
            !read_internal_dword(memory_manager, guest_rsp + 0x48, stored_tag) ||
            !read_internal_dword(memory_manager, guest_rsp + 0x4C, stored_ip) ||
            !read_internal_dword(memory_manager, guest_rsp + 0x50, stored_opcode) ||
            !read_internal_dword(memory_manager, guest_rsp + 0x54, stored_dp) ||
            !read_internal_dword(memory_manager, guest_rsp + 0x58, stored_reserved)) {
            failure.case_name = name;
            failure.detail = "environment readback failed";
            break;
        }

        if (stored_control != control_word || stored_status != status_word || stored_tag != tag_word ||
            stored_ip != static_cast<std::uint32_t>(instruction_pointer & 0xFFFFFFFFu) ||
            stored_opcode != (static_cast<std::uint32_t>(last_opcode) << 16) ||
            stored_dp != static_cast<std::uint32_t>(data_pointer & 0xFFFFFFFFu) ||
            stored_reserved != 0) {
            failure.case_name = name;
            failure.detail = "stored environment mismatch";
            break;
        }

        if (context.x87_control_word != static_cast<std::uint16_t>(control_word | 0x003Fu)) {
            failure.case_name = name;
            failure.detail = "exception mask update mismatch";
            break;
        }
        if (context.x87_pending_exception) {
            failure.case_name = name;
            failure.detail = "pending exception not cleared by masking";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_env_load_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint16_t control_word,
    std::uint16_t status_word,
    std::uint16_t tag_word,
    std::uint16_t last_opcode,
    std::uint64_t instruction_pointer,
    std::uint64_t data_pointer,
    bool expect_pending_after_load,
    bool followup_should_mf,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        if (!write_internal_dword(memory_manager, guest_rsp + 0x40, control_word) ||
            !write_internal_dword(memory_manager, guest_rsp + 0x44, status_word) ||
            !write_internal_dword(memory_manager, guest_rsp + 0x48, tag_word) ||
            !write_internal_dword(memory_manager, guest_rsp + 0x4C, static_cast<std::uint32_t>(instruction_pointer & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 0x50, static_cast<std::uint32_t>(last_opcode) << 16) ||
            !write_internal_dword(memory_manager, guest_rsp + 0x54, static_cast<std::uint32_t>(data_pointer & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 0x58, 0)) {
            failure.case_name = name;
            failure.detail = "environment source write failed";
            break;
        }

        context.x87_control_word = 0x037Fu;
        context.x87_status_word = 0;
        context.x87_tag_word = 0xFFFFu;
        context.x87_last_opcode = 0;
        context.x87_instruction_pointer = 0;
        context.x87_data_pointer = 0;
        context.x87_pending_exception = false;

        const int load_step = cpu_step(&context);
        if (load_step != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "fldenv step failed";
            break;
        }
        if (context.x87_control_word != control_word || context.x87_status_word != status_word ||
            context.x87_tag_word != tag_word || context.x87_last_opcode != last_opcode ||
            context.x87_instruction_pointer != static_cast<std::uint32_t>(instruction_pointer & 0xFFFFFFFFu) ||
            context.x87_data_pointer != static_cast<std::uint32_t>(data_pointer & 0xFFFFFFFFu) ||
            context.x87_pending_exception != expect_pending_after_load) {
            failure.case_name = name;
            failure.detail = "loaded environment mismatch";
            break;
        }

        const int next_step = cpu_step(&context);
        if (followup_should_mf) {
            if (next_step != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_MF) {
                failure.case_name = name;
                failure.detail = "expected delayed x87 #MF";
                break;
            }
        }
        else {
            if (next_step != kCpuStepOk || cpu_has_exception(&context)) {
                failure.case_name = name;
                failure.detail = "follow-up fnstcw step failed";
                break;
            }

            std::uint16_t stored_control = 0;
            if (!read_internal_word(memory_manager, guest_rsp + 0x60, stored_control)) {
                failure.case_name = name;
                failure.detail = "follow-up readback failed";
                break;
            }
            if (stored_control != control_word) {
                failure.case_name = name;
                failure.detail = "follow-up control word mismatch";
                break;
            }
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_internal_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t cr0_value,
    bool pending_exception,
    std::uint32_t expected_exception,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint16_t initial_control = static_cast<std::uint16_t>(0x5100u | (seed & 0x00FFu));
        const std::uint16_t initial_status = static_cast<std::uint16_t>(0xA200u | ((seed >> 8) & 0x00FFu));
        context.control_regs[REG_CR0] = cr0_value;
        context.x87_control_word = initial_control;
        context.x87_status_word = initial_status;
        context.x87_pending_exception = pending_exception;

        const int status = cpu_step(&context);
        if (status != kCpuStepException) {
            failure.case_name = name;
            failure.detail = "expected CPU_STEP_EXCEPTION";
            break;
        }
        if (context.exception.code != expected_exception) {
            failure.case_name = name;
            failure.detail = "unexpected exception code";
            break;
        }
        if (context.regs[REG_RAX] != initial.regs[0]) {
            failure.case_name = name;
            failure.detail = "rax changed unexpectedly";
            break;
        }
        if ((context.rflags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags changed unexpectedly";
            break;
        }
        if (context.x87_control_word != initial_control || context.x87_status_word != initial_status ||
            context.x87_pending_exception != pending_exception) {
            failure.case_name = name;
            failure.detail = "x87 state changed unexpectedly";
            break;
        }

        cpu_clear_exception(&context);
        context.rip = kGuestCodeBase + static_cast<std::uint64_t>(code.size());

        const int resume0 = cpu_step(&context);
        if (resume0 != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "resume step 0 failed";
            break;
        }
        const int resume1 = cpu_step(&context);
        if (resume1 != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "resume step 1 failed";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_x87_public_cr0_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t cr0_value,
    std::uint32_t expected_exception,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_CR0, cr0_value)) {
            failure.case_name = name;
            failure.detail = "cr0 write failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_EXCEPTION) {
            failure.case_name = name;
            failure.detail = "expected CPUEAXH_ERR_EXCEPTION";
            break;
        }
        if (cpueaxh_code_exception(engine) != expected_exception) {
            failure.case_name = name;
            failure.detail = "unexpected exception code";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_public_cr4_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t cr4_value,
    std::uint32_t expected_exception,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_reg(engine, CPUEAXH_X86_REG_CR4, cr4_value)) {
            failure.case_name = name;
            failure.detail = "cr4 write failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_EXCEPTION) {
            failure.case_name = name;
            failure.detail = "expected CPUEAXH_ERR_EXCEPTION";
            break;
        }
        if (cpueaxh_code_exception(engine) != expected_exception) {
            failure.case_name = name;
            failure.detail = "unexpected exception code";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_manual_cr4_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint64_t cr4_value,
    std::uint32_t expected_exception,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }
        context.control_regs[4] = cr4_value;

        const int status = cpu_step(&context);
        if (status != kCpuStepException) {
            failure.case_name = name;
            failure.detail = "expected CPU_STEP_EXCEPTION";
            break;
        }
        if (context.exception.code != expected_exception) {
            failure.case_name = name;
            failure.detail = "unexpected exception code";
            break;
        }
        if (context.regs[REG_RAX] != initial.regs[0]) {
            failure.case_name = name;
            failure.detail = "rax changed unexpectedly";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint32_t expected_exception,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        const int status = cpu_step(&context);
        if (status != kCpuStepException) {
            failure.case_name = name;
            failure.detail = "expected CPU_STEP_EXCEPTION";
            break;
        }
        if (context.exception.code != expected_exception) {
            failure.case_name = name;
            failure.detail = "unexpected exception code";
            break;
        }

        if (context.regs[REG_RAX] != initial.regs[0]) {
            failure.case_name = name;
            failure.detail = "rax changed unexpectedly";
            break;
        }
        if ((context.rflags & kStatusMask) != (initial.rflags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags changed unexpectedly";
            break;
        }
        if (context.regs[REG_RSP] != guest_rsp) {
            failure.case_name = name;
            failure.detail = "rsp changed unexpectedly";
            break;
        }
        if (context.rip != kGuestCodeBase) {
            failure.case_name = name;
            failure.detail = "rip changed unexpectedly";
            break;
        }

        cpu_clear_exception(&context);
        context.rip = kGuestCodeBase + static_cast<std::uint64_t>(code.size());

        const int resume0 = cpu_step(&context);
        if (resume0 != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "resume step 0 failed";
            break;
        }
        const std::uint64_t expected_rip_after_resume0 = kGuestCodeBase + static_cast<std::uint64_t>(code.size()) + 1;
        if (context.rip != expected_rip_after_resume0) {
            failure.case_name = name;
            failure.detail = "resume rip 0 mismatch";
            break;
        }

        const int resume1 = cpu_step(&context);
        if (resume1 != kCpuStepOk || cpu_has_exception(&context)) {
            failure.case_name = name;
            failure.detail = "resume step 1 failed";
            break;
        }
        const std::uint64_t expected_rip_after_resume1 = expected_rip_after_resume0 + 1;
        if (context.rip != expected_rip_after_resume1) {
            failure.case_name = name;
            failure.detail = "resume rip 1 mismatch";
            break;
        }
        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_movs_source_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::vector<std::uint8_t> code = { 0x48, 0xA5, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        constexpr std::uint64_t source_address = kGuestGdtBase;
        const std::uint64_t dest_address = guest_rsp + 0x40;
        constexpr std::uint64_t sentinel = 0x1122334455667788ull;
        if (!write_internal_dword(memory_manager, dest_address, static_cast<std::uint32_t>(sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, dest_address + 4, static_cast<std::uint32_t>(sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "sentinel write failed";
            break;
        }

        context.regs[REG_RSI] = source_address;
        context.regs[REG_RDI] = dest_address;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected MOVS source #PF";
            break;
        }
        if (context.rip != kGuestCodeBase ||
            context.regs[REG_RSI] != source_address ||
            context.regs[REG_RDI] != dest_address ||
            context.regs[REG_RSP] != guest_rsp) {
            failure.case_name = name;
            failure.detail = "MOVS scalar state changed after #PF";
            break;
        }

        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!read_internal_dword(memory_manager, dest_address, low) ||
            !read_internal_dword(memory_manager, dest_address + 4, high)) {
            failure.case_name = name;
            failure.detail = "sentinel readback failed";
            break;
        }
        const std::uint64_t actual = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        if (actual != sentinel) {
            failure.case_name = name;
            failure.detail = "MOVS destination changed after source #PF";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_rep_movs_source_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t source_page = kGuestGdtBase;
        const std::uint64_t source_linear = source_page + kCodePageSize - 16;
        const std::uint64_t dest_linear = kGuestStackBase + 0x100;
        const std::uint64_t source_value0 = seeded(seed, 0xE181);
        const std::uint64_t source_value1 = seeded(seed, 0xE182);
        const std::uint64_t dest_sentinel = 0x8877665544332211ull;
        const std::vector<std::uint8_t> code = { 0xF3, 0x48, 0xA5, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!mm_map_internal(&memory_manager, source_page, kCodePageSize, MM_PROT_READ | MM_PROT_WRITE)) {
            failure.case_name = name;
            failure.detail = "REP MOVS split source map failed";
            break;
        }
        if (!write_internal_dword(memory_manager, source_linear, static_cast<std::uint32_t>(source_value0 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, source_linear + 4, static_cast<std::uint32_t>(source_value0 >> 32)) ||
            !write_internal_dword(memory_manager, source_linear + 8, static_cast<std::uint32_t>(source_value1 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, source_linear + 12, static_cast<std::uint32_t>(source_value1 >> 32)) ||
            !write_internal_dword(memory_manager, dest_linear, static_cast<std::uint32_t>(dest_sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, dest_linear + 4, static_cast<std::uint32_t>(dest_sentinel >> 32)) ||
            !write_internal_dword(memory_manager, dest_linear + 8, static_cast<std::uint32_t>(dest_sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, dest_linear + 12, static_cast<std::uint32_t>(dest_sentinel >> 32)) ||
            !write_internal_dword(memory_manager, dest_linear + 16, static_cast<std::uint32_t>(dest_sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, dest_linear + 20, static_cast<std::uint32_t>(dest_sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "REP MOVS split setup write failed";
            break;
        }

        const std::uint64_t source_index = source_linear - cpu_segment_base_for_addressing(&context, SEG_DS);
        const std::uint64_t dest_index = dest_linear - cpu_segment_base_for_addressing(&context, SEG_ES);
        context.regs[REG_RSI] = source_index;
        context.regs[REG_RDI] = dest_index;
        context.regs[REG_RCX] = 3;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected REP MOVS split source #PF";
            break;
        }
        if (context.rip != kGuestCodeBase ||
            context.regs[REG_RSI] != source_index + 16 ||
            context.regs[REG_RDI] != dest_index + 16 ||
            context.regs[REG_RCX] != 1 ||
            context.regs[REG_RSP] != guest_rsp) {
            failure.case_name = name;
            failure.detail = std::string("REP MOVS split source exception changed scalar state incorrectly rsi=") +
                std::to_string(context.regs[REG_RSI]) + " rdi=" + std::to_string(context.regs[REG_RDI]) +
                " rcx=" + std::to_string(context.regs[REG_RCX]) + " rsp=" + std::to_string(context.regs[REG_RSP]) +
                " rip=" + std::to_string(context.rip) + " cr2=" + std::to_string(context.control_regs[REG_CR2]);
            break;
        }
        if (context.rex_present ||
            context.rex_w ||
            context.rex_r ||
            context.rex_x ||
            context.rex_b ||
            context.operand_size_override ||
            context.address_size_override ||
            context.segment_override != -1) {
            failure.case_name = name;
            failure.detail = "REP MOVS split source exception left decode transient state live";
            break;
        }

        std::uint32_t low0 = 0;
        std::uint32_t high0 = 0;
        std::uint32_t low1 = 0;
        std::uint32_t high1 = 0;
        std::uint32_t low2 = 0;
        std::uint32_t high2 = 0;
        if (!read_internal_dword(memory_manager, dest_linear, low0) ||
            !read_internal_dword(memory_manager, dest_linear + 4, high0) ||
            !read_internal_dword(memory_manager, dest_linear + 8, low1) ||
            !read_internal_dword(memory_manager, dest_linear + 12, high1) ||
            !read_internal_dword(memory_manager, dest_linear + 16, low2) ||
            !read_internal_dword(memory_manager, dest_linear + 20, high2)) {
            failure.case_name = name;
            failure.detail = "REP MOVS split destination readback failed";
            break;
        }
        const std::uint64_t copied0 = static_cast<std::uint64_t>(low0) | (static_cast<std::uint64_t>(high0) << 32);
        const std::uint64_t copied1 = static_cast<std::uint64_t>(low1) | (static_cast<std::uint64_t>(high1) << 32);
        const std::uint64_t untouched = static_cast<std::uint64_t>(low2) | (static_cast<std::uint64_t>(high2) << 32);
        if (copied0 != source_value0 || copied1 != source_value1 || untouched != dest_sentinel) {
            failure.case_name = name;
            failure.detail = "REP MOVS split destination commit mismatch";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_rep_movs_dest_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t source_page = kGuestGdtBase;
        const std::uint64_t source_linear = source_page + 0x100;
        const std::uint64_t dest_linear = kGuestStackBase + kStackSize - 20;
        const std::uint64_t source_value0 = seeded(seed, 0xE191);
        const std::uint64_t source_value1 = seeded(seed, 0xE192);
        const std::uint64_t source_value2 = seeded(seed, 0xE193);
        const std::uint64_t dest_sentinel0 = 0x8877665544332211ull;
        const std::uint32_t dest_sentinel2_low = 0xA5A55A5Au;
        const std::vector<std::uint8_t> code = { 0xF3, 0x48, 0xA5, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!mm_map_internal(&memory_manager, source_page, kCodePageSize, MM_PROT_READ | MM_PROT_WRITE)) {
            failure.case_name = name;
            failure.detail = "REP MOVS split dest source map failed";
            break;
        }
        if (!write_internal_dword(memory_manager, source_linear, static_cast<std::uint32_t>(source_value0 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, source_linear + 4, static_cast<std::uint32_t>(source_value0 >> 32)) ||
            !write_internal_dword(memory_manager, source_linear + 8, static_cast<std::uint32_t>(source_value1 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, source_linear + 12, static_cast<std::uint32_t>(source_value1 >> 32)) ||
            !write_internal_dword(memory_manager, source_linear + 16, static_cast<std::uint32_t>(source_value2 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, source_linear + 20, static_cast<std::uint32_t>(source_value2 >> 32)) ||
            !write_internal_dword(memory_manager, dest_linear, static_cast<std::uint32_t>(dest_sentinel0 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, dest_linear + 4, static_cast<std::uint32_t>(dest_sentinel0 >> 32)) ||
            !write_internal_dword(memory_manager, dest_linear + 8, static_cast<std::uint32_t>(dest_sentinel0 & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, dest_linear + 12, static_cast<std::uint32_t>(dest_sentinel0 >> 32)) ||
            !write_internal_dword(memory_manager, dest_linear + 16, dest_sentinel2_low)) {
            failure.case_name = name;
            failure.detail = "REP MOVS split dest setup write failed";
            break;
        }

        const std::uint64_t source_index = source_linear - cpu_segment_base_for_addressing(&context, SEG_DS);
        const std::uint64_t dest_index = dest_linear - cpu_segment_base_for_addressing(&context, SEG_ES);
        context.regs[REG_RSI] = source_index;
        context.regs[REG_RDI] = dest_index;
        context.regs[REG_RCX] = 3;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected REP MOVS split dest #PF";
            break;
        }
        if (context.rip != kGuestCodeBase ||
            context.regs[REG_RSI] != source_index + 16 ||
            context.regs[REG_RDI] != dest_index + 16 ||
            context.regs[REG_RCX] != 1 ||
            context.regs[REG_RSP] != guest_rsp) {
            failure.case_name = name;
            failure.detail = "REP MOVS split dest exception changed scalar state incorrectly";
            break;
        }

        std::uint32_t low0 = 0;
        std::uint32_t high0 = 0;
        std::uint32_t low1 = 0;
        std::uint32_t high1 = 0;
        std::uint32_t low2 = 0;
        if (!read_internal_dword(memory_manager, dest_linear, low0) ||
            !read_internal_dword(memory_manager, dest_linear + 4, high0) ||
            !read_internal_dword(memory_manager, dest_linear + 8, low1) ||
            !read_internal_dword(memory_manager, dest_linear + 12, high1) ||
            !read_internal_dword(memory_manager, dest_linear + 16, low2)) {
            failure.case_name = name;
            failure.detail = "REP MOVS split dest readback failed";
            break;
        }
        const std::uint64_t copied0 = static_cast<std::uint64_t>(low0) | (static_cast<std::uint64_t>(high0) << 32);
        const std::uint64_t copied1 = static_cast<std::uint64_t>(low1) | (static_cast<std::uint64_t>(high1) << 32);
        if (copied0 != source_value0 || copied1 != source_value1 || low2 != dest_sentinel2_low) {
            failure.case_name = name;
            failure.detail = "REP MOVS split dest commit mismatch";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_stack_source_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    bool use_unmapped_rbp,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t saved_rax = context.regs[REG_RAX];
        const std::uint64_t saved_rbp = context.regs[REG_RBP];
        if (use_unmapped_rbp) {
            context.regs[REG_RBP] = kGuestGdtBase;
        }
        else {
            context.regs[REG_RSP] = kGuestGdtBase;
        }
        const std::uint64_t expected_rsp = context.regs[REG_RSP];
        const std::uint64_t expected_rbp = context.regs[REG_RBP];

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected stack source #PF";
            break;
        }
        if (context.rip != kGuestCodeBase ||
            context.regs[REG_RSP] != expected_rsp ||
            context.regs[REG_RBP] != expected_rbp ||
            context.regs[REG_RAX] != saved_rax) {
            failure.case_name = name;
            failure.detail = "stack source exception changed scalar state";
            break;
        }
        if (!use_unmapped_rbp && context.regs[REG_RBP] != saved_rbp) {
            failure.case_name = name;
            failure.detail = "rbp changed unexpectedly";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_stack_write_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t saved_rax = context.regs[REG_RAX];
        context.regs[REG_RSP] = kGuestGdtBase;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected stack write #PF";
            break;
        }
        if (context.rip != kGuestCodeBase ||
            context.regs[REG_RSP] != kGuestGdtBase ||
            context.regs[REG_RAX] != saved_rax) {
            failure.case_name = name;
            failure.detail = "stack write exception changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_xmm_store_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t split_address = kGuestStackBase + kStackSize - 8;
        const std::uint64_t sentinel = 0x8877665544332211ull;
        const std::vector<std::uint8_t> code = {
            0x0F, 0x11, 0x04, 0x25,
            static_cast<std::uint8_t>(split_address & 0xFFu),
            static_cast<std::uint8_t>((split_address >> 8) & 0xFFu),
            static_cast<std::uint8_t>((split_address >> 16) & 0xFFu),
            static_cast<std::uint8_t>((split_address >> 24) & 0xFFu),
            0x90, 0x90
        };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        if (!write_internal_dword(memory_manager, split_address, static_cast<std::uint32_t>(sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, split_address + 4, static_cast<std::uint32_t>(sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "split sentinel write failed";
            break;
        }

        context.xmm[0].low = 0x0102030405060708ull;
        context.xmm[0].high = 0x1112131415161718ull;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected split XMM store #PF";
            break;
        }
        if (context.rip != kGuestCodeBase || context.regs[REG_RSP] != guest_rsp) {
            failure.case_name = name;
            failure.detail = "split XMM store changed scalar state";
            break;
        }

        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!read_internal_dword(memory_manager, split_address, low) ||
            !read_internal_dword(memory_manager, split_address + 4, high)) {
            failure.case_name = name;
            failure.detail = "split sentinel readback failed";
            break;
        }
        const std::uint64_t actual = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        if (actual != sentinel) {
            failure.case_name = name;
            failure.detail = "split XMM store partially wrote first page";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_maskmovdqu_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t split_address = kGuestStackBase + kStackSize - 8;
        const std::uint64_t sentinel = 0x1020304050607080ull;
        const std::vector<std::uint8_t> code = {
            0x66, 0x0F, 0xF7, 0xC1,
            0x90, 0x90
        };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        if (!write_internal_dword(memory_manager, split_address, static_cast<std::uint32_t>(sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, split_address + 4, static_cast<std::uint32_t>(sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "maskmov sentinel write failed";
            break;
        }

        context.regs[REG_RDI] = split_address;
        context.xmm[0].low = 0x0102030405060708ull;
        context.xmm[0].high = 0x1112131415161718ull;
        context.xmm[1].low = 0x8080808080808080ull;
        context.xmm[1].high = 0x8080808080808080ull;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected split MASKMOVDQU #PF";
            break;
        }
        if (context.rip != kGuestCodeBase ||
            context.regs[REG_RSP] != guest_rsp ||
            context.regs[REG_RDI] != split_address) {
            failure.case_name = name;
            failure.detail = "split MASKMOVDQU changed scalar state";
            break;
        }

        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!read_internal_dword(memory_manager, split_address, low) ||
            !read_internal_dword(memory_manager, split_address + 4, high)) {
            failure.case_name = name;
            failure.detail = "maskmov sentinel readback failed";
            break;
        }
        const std::uint64_t actual = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        if (actual != sentinel) {
            failure.case_name = name;
            failure.detail = "split MASKMOVDQU partially wrote first page";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline void append_abs32(std::vector<std::uint8_t>& code, std::uint64_t address) {
    code.push_back(static_cast<std::uint8_t>(address & 0xFFu));
    code.push_back(static_cast<std::uint8_t>((address >> 8) & 0xFFu));
    code.push_back(static_cast<std::uint8_t>((address >> 16) & 0xFFu));
    code.push_back(static_cast<std::uint8_t>((address >> 24) & 0xFFu));
}

inline bool run_x87_env_store_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t split_address = kGuestStackBase + kStackSize - 16;
        const std::uint32_t sentinel[4] = { 0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u };
        std::vector<std::uint8_t> code = { 0xD9, 0x34, 0x25 };
        append_abs32(code, split_address);
        code.push_back(0x90);
        code.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        for (int index = 0; index < 4; ++index) {
            if (!write_internal_dword(memory_manager, split_address + static_cast<std::uint64_t>(index * 4), sentinel[index])) {
                failure.case_name = name;
                failure.detail = "x87 env sentinel write failed";
                goto done;
            }
        }

        context.x87_control_word = 0x1234u;
        context.x87_status_word = 0x5678u;
        context.x87_tag_word = 0x9ABCu;
        context.x87_last_opcode = 0x0DEFu;
        context.x87_instruction_pointer = 0xCAFEBABEull;
        context.x87_data_pointer = 0x10203040ull;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected split FNSTENV #PF";
            break;
        }
        if (context.rip != kGuestCodeBase || context.x87_control_word != 0x1234u) {
            failure.case_name = name;
            failure.detail = "split FNSTENV changed CPU state";
            break;
        }

        for (int index = 0; index < 4; ++index) {
            std::uint32_t actual = 0;
            if (!read_internal_dword(memory_manager, split_address + static_cast<std::uint64_t>(index * 4), actual) ||
                actual != sentinel[index]) {
                failure.case_name = name;
                failure.detail = "split FNSTENV partially wrote first page";
                goto done;
            }
        }

        ok = true;
    } while (false);

done:
    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_x87_env_load_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t split_address = kGuestStackBase + kStackSize - 16;
        std::vector<std::uint8_t> code = { 0xD9, 0x24, 0x25 };
        append_abs32(code, split_address);
        code.push_back(0x90);
        code.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        if (!write_internal_dword(memory_manager, split_address + 0x00, 0x00001111u) ||
            !write_internal_dword(memory_manager, split_address + 0x04, 0x00002222u) ||
            !write_internal_dword(memory_manager, split_address + 0x08, 0x00003333u) ||
            !write_internal_dword(memory_manager, split_address + 0x0C, 0x44445555u)) {
            failure.case_name = name;
            failure.detail = "x87 env source write failed";
            break;
        }

        context.x87_control_word = 0xAAAAu;
        context.x87_status_word = 0xBBBBu;
        context.x87_tag_word = 0xCCCCu;
        context.x87_last_opcode = 0xDDDDu;
        context.x87_instruction_pointer = 0x11112222ull;
        context.x87_data_pointer = 0x33334444ull;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected split FLDENV #PF";
            break;
        }
        if (context.x87_control_word != 0xAAAAu ||
            context.x87_status_word != 0xBBBBu ||
            context.x87_tag_word != 0xCCCCu ||
            context.x87_last_opcode != 0xDDDDu ||
            context.x87_instruction_pointer != 0x11112222ull ||
            context.x87_data_pointer != 0x33334444ull) {
            failure.case_name = name;
            failure.detail = "split FLDENV partially changed x87 state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_fxsave_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t split_address = kGuestStackBase + kStackSize - 0x20;
        std::uint32_t sentinel[8] = {};
        std::vector<std::uint8_t> code = { 0x0F, 0xAE, 0x04, 0x25 };
        append_abs32(code, split_address);
        code.push_back(0x90);
        code.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        for (int index = 0; index < 8; ++index) {
            sentinel[index] = static_cast<std::uint32_t>(0xA0B0C000u + index);
            if (!write_internal_dword(memory_manager, split_address + static_cast<std::uint64_t>(index * 4), sentinel[index])) {
                failure.case_name = name;
                failure.detail = "fxsave sentinel write failed";
                goto done;
            }
        }

        context.mxcsr = 0x1F80u;
        context.mm[0] = 0x0102030405060708ull;
        context.xmm[0].low = 0x1112131415161718ull;
        context.xmm[0].high = 0x2122232425262728ull;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected split FXSAVE #PF";
            break;
        }

        for (int index = 0; index < 8; ++index) {
            std::uint32_t actual = 0;
            if (!read_internal_dword(memory_manager, split_address + static_cast<std::uint64_t>(index * 4), actual) ||
                actual != sentinel[index]) {
                failure.case_name = name;
                failure.detail = "split FXSAVE partially wrote first page";
                goto done;
            }
        }

        ok = true;
    } while (false);

done:
    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_fxrstor_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t split_address = kGuestStackBase + kStackSize - 0x20;
        std::vector<std::uint8_t> code = { 0x0F, 0xAE, 0x0C, 0x25 };
        append_abs32(code, split_address);
        code.push_back(0x90);
        code.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        if (!write_internal_dword(memory_manager, split_address + 0x18, 0x00001F80u)) {
            failure.case_name = name;
            failure.detail = "fxrstor mxcsr source write failed";
            break;
        }

        context.mxcsr = 0x00001F81u;
        context.mm[0] = 0xAAAAAAAAAAAAAAAAull;
        context.xmm[0].low = 0xBBBBBBBBBBBBBBBBull;
        context.xmm[0].high = 0xCCCCCCCCCCCCCCCCull;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected split FXRSTOR #PF";
            break;
        }
        if (context.mxcsr != 0x00001F81u ||
            context.mm[0] != 0xAAAAAAAAAAAAAAAAull ||
            context.xmm[0].low != 0xBBBBBBBBBBBBBBBBull ||
            context.xmm[0].high != 0xCCCCCCCCCCCCCCCCull) {
            failure.case_name = name;
            failure.detail = "split FXRSTOR partially changed SIMD state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_far_call_stack_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + 8;
        const std::uint64_t pointer_address = kGuestStackBase + 0x100;
        const std::uint64_t sentinel = 0x8877665544332211ull;
        const std::uint64_t target = kGuestCodeBase + 0x80;
        std::vector<std::uint8_t> code = { 0x48, 0xFF, 0x1C, 0x25 };
        append_abs32(code, pointer_address);
        code.push_back(0x90);
        code.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!mm_map_internal(&memory_manager, kGuestGdtBase, kCodePageSize, MM_PROT_READ | MM_PROT_WRITE)) {
            failure.case_name = name;
            failure.detail = "far call gdt map failed";
            break;
        }

        const std::uint64_t descriptor = encode_segment_descriptor(0, 0x000FFFFFu, 0x0Bu, 0, true, true, false, true);
        if (!write_internal_dword(memory_manager, kGuestGdtBase + 0x08, static_cast<std::uint32_t>(descriptor & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, kGuestGdtBase + 0x0C, static_cast<std::uint32_t>(descriptor >> 32)) ||
            !write_internal_dword(memory_manager, pointer_address, static_cast<std::uint32_t>(target & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, pointer_address + 4, static_cast<std::uint32_t>(target >> 32)) ||
            !write_internal_word(memory_manager, pointer_address + 8, 0x08u) ||
            !write_internal_dword(memory_manager, kGuestStackBase, static_cast<std::uint32_t>(sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, kGuestStackBase + 4, static_cast<std::uint32_t>(sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "far call setup write failed";
            break;
        }

        context.gdtr_base = kGuestGdtBase;
        context.gdtr_limit = 0x0F;
        context.cs.selector = 0x08u;
        context.cs.descriptor.base = 0;
        context.cs.descriptor.limit = 0xFFFFFFFFu;
        context.cs.descriptor.type = 0x0Bu;
        context.cs.descriptor.dpl = 0;
        context.cs.descriptor.present = true;
        context.cs.descriptor.granularity = true;
        context.cs.descriptor.db = false;
        context.cs.descriptor.long_mode = true;
        context.cached_mode_key_valid = 0;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = std::string("expected far CALL split stack #PF status=") + std::to_string(status) +
                " exc=" + std::to_string(context.exception.code);
            break;
        }
        if (context.rip != kGuestCodeBase || context.regs[REG_RSP] != guest_rsp) {
            failure.case_name = name;
            failure.detail = "far CALL split stack changed scalar state";
            break;
        }

        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!read_internal_dword(memory_manager, kGuestStackBase, low) ||
            !read_internal_dword(memory_manager, kGuestStackBase + 4, high)) {
            failure.case_name = name;
            failure.detail = "far call sentinel readback failed";
            break;
        }
        const std::uint64_t actual = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        if (actual != sentinel) {
            failure.case_name = name;
            failure.detail = "far CALL partially wrote first stack slot";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_multi_pop_stack_split_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kStackSize - 8;
        const std::uint64_t first_value = kGuestCodeBase + 0x80;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_internal_dword(memory_manager, guest_rsp, static_cast<std::uint32_t>(first_value & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 4, static_cast<std::uint32_t>(first_value >> 32))) {
            failure.case_name = name;
            failure.detail = "multi-pop stack setup write failed";
            break;
        }

        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rip = context.rip;
        const std::uint64_t saved_rflags = context.rflags;
        const std::uint16_t saved_cs = context.cs.selector;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = std::string("expected multi-pop split stack #PF status=") + std::to_string(status) +
                " exc=" + std::to_string(context.exception.code);
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.rflags != saved_rflags ||
            context.cs.selector != saved_cs) {
            failure.case_name = name;
            failure.detail = "multi-pop split stack changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_enter_stack_write_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + 4;
        const std::vector<std::uint8_t> code = { 0xC8, 0x00, 0x00, 0x00, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rbp = context.regs[REG_RBP];
        const std::uint64_t saved_rip = context.rip;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected ENTER stack write #PF";
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.regs[REG_RBP] != saved_rbp) {
            failure.case_name = name;
            failure.detail = "ENTER stack write exception changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_enter_nested_frame_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t first_push_address = guest_rsp - 8;
        const std::uint64_t sentinel = 0xA5A5CC3301020304ull;
        const std::vector<std::uint8_t> code = { 0xC8, 0x00, 0x00, 0x02, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_internal_dword(memory_manager, first_push_address, static_cast<std::uint32_t>(sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, first_push_address + 4, static_cast<std::uint32_t>(sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "ENTER nested setup write failed";
            break;
        }

        context.regs[REG_RBP] = kGuestStackBase + kStackSize + 4;
        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rbp = context.regs[REG_RBP];
        const std::uint64_t saved_rip = context.rip;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected ENTER nested frame #PF";
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.regs[REG_RBP] != saved_rbp) {
            failure.case_name = name;
            failure.detail = "ENTER nested frame exception changed scalar state";
            break;
        }

        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!read_internal_dword(memory_manager, first_push_address, low) ||
            !read_internal_dword(memory_manager, first_push_address + 4, high)) {
            failure.case_name = name;
            failure.detail = "ENTER nested sentinel readback failed";
            break;
        }
        const std::uint64_t actual = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        if (actual != sentinel) {
            failure.case_name = name;
            failure.detail = "ENTER nested frame fault partially wrote stack";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_leave_frame_source_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::vector<std::uint8_t> code = { 0xC9, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        context.regs[REG_RBP] = kGuestStackBase + kStackSize;
        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rbp = context.regs[REG_RBP];
        const std::uint64_t saved_rip = context.rip;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected LEAVE frame source #PF";
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.regs[REG_RBP] != saved_rbp) {
            failure.case_name = name;
            failure.detail = "LEAVE frame source exception changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_pop_memory_dest_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t stack_value = seeded(seed, 0xE171);
        const std::uint64_t dest_address = kGuestStackBase + kStackSize;
        std::vector<std::uint8_t> code = { 0x8F, 0x04, 0x25 };
        append_abs32(code, dest_address);
        code.push_back(0x90);
        code.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_internal_dword(memory_manager, guest_rsp, static_cast<std::uint32_t>(stack_value & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 4, static_cast<std::uint32_t>(stack_value >> 32))) {
            failure.case_name = name;
            failure.detail = "POP memory dest setup write failed";
            break;
        }

        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rip = context.rip;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_PF) {
            failure.case_name = name;
            failure.detail = "expected POP memory dest #PF";
            break;
        }
        if (context.rip != saved_rip || context.regs[REG_RSP] != saved_rsp) {
            failure.case_name = name;
            failure.detail = "POP memory dest exception changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_pop_sreg_invalid_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint16_t invalid_selector = 0x20u;
        const std::vector<std::uint8_t> code = { 0x0F, 0xA1, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_internal_dword(memory_manager, guest_rsp, invalid_selector) ||
            !write_internal_dword(memory_manager, guest_rsp + 4, 0)) {
            failure.case_name = name;
            failure.detail = "POP sreg setup write failed";
            break;
        }

        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rip = context.rip;
        const std::uint16_t saved_fs = context.fs.selector;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_GP) {
            failure.case_name = name;
            failure.detail = "expected POP sreg #GP";
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.fs.selector != saved_fs) {
            failure.case_name = name;
            failure.detail = "POP sreg exception changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_noncanonical_control_transfer_exception_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    bool target_in_rax,
    Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        constexpr std::uint64_t noncanonical_target = 0x0000800000000000ull;
        constexpr std::uint64_t stack_sentinel = 0xD1D2D3D4A1A2A3A4ull;
        std::vector<std::uint8_t> image = code;
        image.push_back(0x90);
        image.push_back(0x90);
        if (!initialize_manual_cpu_context(context, memory_manager, image, initial, guest_rsp, failure, name)) {
            break;
        }

        if (target_in_rax) {
            context.regs[REG_RAX] = noncanonical_target;
        }
        else if (!write_internal_dword(memory_manager, guest_rsp, static_cast<std::uint32_t>(noncanonical_target & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 4, static_cast<std::uint32_t>(noncanonical_target >> 32))) {
            failure.case_name = name;
            failure.detail = "noncanonical RET stack setup write failed";
            break;
        }

        if (!write_internal_dword(memory_manager, guest_rsp - 8, static_cast<std::uint32_t>(stack_sentinel & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp - 4, static_cast<std::uint32_t>(stack_sentinel >> 32))) {
            failure.case_name = name;
            failure.detail = "noncanonical control stack sentinel write failed";
            break;
        }

        const std::uint64_t saved_rip = context.rip;
        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rax = context.regs[REG_RAX];
        const std::uint64_t saved_rflags = context.rflags;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_GP) {
            failure.case_name = name;
            failure.detail = "expected noncanonical control transfer #GP";
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.regs[REG_RAX] != saved_rax ||
            context.rflags != saved_rflags) {
            failure.case_name = name;
            failure.detail = "noncanonical control transfer changed scalar state";
            break;
        }

        std::uint32_t low = 0;
        std::uint32_t high = 0;
        if (!read_internal_dword(memory_manager, guest_rsp - 8, low) ||
            !read_internal_dword(memory_manager, guest_rsp - 4, high)) {
            failure.case_name = name;
            failure.detail = "noncanonical control stack sentinel readback failed";
            break;
        }
        const std::uint64_t actual_sentinel = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
        if (actual_sentinel != stack_sentinel) {
            failure.case_name = name;
            failure.detail = "noncanonical CALL partially wrote stack";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_iret_invalid_selector_exception_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    MEMORY_MANAGER memory_manager = {};
    CPU_CONTEXT context = {};
    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::vector<std::uint8_t> code = { 0xCF, 0x90, 0x90 };
        if (!initialize_manual_cpu_context(context, memory_manager, code, initial, guest_rsp, failure, name)) {
            break;
        }

        const std::uint64_t target_rip = kGuestCodeBase + 0x80;
        const std::uint16_t invalid_selector = 0x20u;
        if (!write_internal_dword(memory_manager, guest_rsp, static_cast<std::uint32_t>(target_rip & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 4, static_cast<std::uint32_t>(target_rip >> 32)) ||
            !write_internal_dword(memory_manager, guest_rsp + 8, invalid_selector) ||
            !write_internal_dword(memory_manager, guest_rsp + 12, 0) ||
            !write_internal_dword(memory_manager, guest_rsp + 16, static_cast<std::uint32_t>(initial.rflags & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 20, static_cast<std::uint32_t>(initial.rflags >> 32)) ||
            !write_internal_dword(memory_manager, guest_rsp + 24, static_cast<std::uint32_t>(guest_rsp & 0xFFFFFFFFu)) ||
            !write_internal_dword(memory_manager, guest_rsp + 28, static_cast<std::uint32_t>(guest_rsp >> 32)) ||
            !write_internal_dword(memory_manager, guest_rsp + 32, context.ss.selector) ||
            !write_internal_dword(memory_manager, guest_rsp + 36, 0)) {
            failure.case_name = name;
            failure.detail = "IRET invalid selector stack setup write failed";
            break;
        }

        const std::uint64_t saved_rip = context.rip;
        const std::uint64_t saved_rsp = context.regs[REG_RSP];
        const std::uint64_t saved_rflags = context.rflags;
        const std::uint16_t saved_cs = context.cs.selector;
        const SegmentDescriptor saved_cs_desc = context.cs.descriptor;

        const int status = cpu_step(&context);
        if (status != kCpuStepException || context.exception.code != CPUEAXH_EXCEPTION_GP) {
            failure.case_name = name;
            failure.detail = "expected IRET invalid selector #GP";
            break;
        }
        if (context.rip != saved_rip ||
            context.regs[REG_RSP] != saved_rsp ||
            context.rflags != saved_rflags ||
            context.cs.selector != saved_cs ||
            context.cs.descriptor.type != saved_cs_desc.type ||
            context.cs.descriptor.long_mode != saved_cs_desc.long_mode) {
            failure.case_name = name;
            failure.detail = "IRET invalid selector changed scalar state";
            break;
        }

        ok = true;
    } while (false);

    mm_destroy(&memory_manager);
    return ok;
}

inline bool run_manual_exception_case_public(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint32_t expected_exception,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_EXCEPTION) {
            failure.case_name = name;
            failure.detail = "expected CPUEAXH_ERR_EXCEPTION";
            break;
        }
        if (cpueaxh_code_exception(engine) != expected_exception) {
            failure.case_name = name;
            failure.detail = "unexpected exception code";
            break;
        }
        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

struct ManualInsEscapeState {
    std::vector<std::uint32_t> values;
    std::uint16_t expected_port = 0;
};

inline void write_manual_ins_index(cpueaxh_x86_context* context, int address_size, std::uint64_t value) {
    if (address_size == 32) {
        context->regs[CPUEAXH_X86_REG_RDI] = static_cast<std::uint32_t>(value);
        return;
    }
    context->regs[CPUEAXH_X86_REG_RDI] = value;
}

inline void write_manual_ins_count(cpueaxh_x86_context* context, int address_size, std::uint64_t value) {
    if (address_size == 32) {
        context->regs[CPUEAXH_X86_REG_RCX] = static_cast<std::uint32_t>(value);
        return;
    }
    context->regs[CPUEAXH_X86_REG_RCX] = value;
}

inline cpueaxh_err write_manual_ins_value(cpueaxh_engine* engine, std::uint64_t address, int operand_bytes, std::uint32_t value) {
    std::uint8_t buffer[4] = {};
    for (int index = 0; index < operand_bytes; ++index) {
        buffer[index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFu);
    }
    return cpueaxh_mem_write(engine, address, buffer, static_cast<std::size_t>(operand_bytes));
}

inline cpueaxh_err manual_ins_escape_callback(
    cpueaxh_engine* engine,
    cpueaxh_x86_context* context,
    const std::uint8_t* instruction,
    void* user_data) {
    ManualInsEscapeState* state = reinterpret_cast<ManualInsEscapeState*>(user_data);
    if (!engine || !context || !instruction || !state) {
        return CPUEAXH_ERR_ARG;
    }

    std::size_t offset = 0;
    bool operand_size_override = false;
    bool address_size_override = false;
    bool has_lock_prefix = false;
    std::uint8_t rep_prefix = 0;
    while (true) {
        const std::uint8_t prefix = instruction[offset];
        if (prefix == 0x66) {
            operand_size_override = true;
            ++offset;
        }
        else if (prefix == 0x67) {
            address_size_override = true;
            ++offset;
        }
        else if (prefix == 0xF0) {
            has_lock_prefix = true;
            ++offset;
        }
        else if (prefix == 0xF2 || prefix == 0xF3) {
            rep_prefix = prefix;
            ++offset;
        }
        else if (prefix == 0x26 || prefix == 0x2E || prefix == 0x36 || prefix == 0x3E ||
                 prefix == 0x64 || prefix == 0x65 ||
                 (prefix >= 0x40 && prefix <= 0x4F)) {
            ++offset;
        }
        else {
            break;
        }
    }

    const std::uint8_t opcode = instruction[offset];
    if (opcode != 0x6C && opcode != 0x6D) {
        return CPUEAXH_ERR_ARG;
    }
    if (has_lock_prefix) {
        context->code_exception = CPUEAXH_EXCEPTION_UD;
        context->error_code_exception = 0;
        return CPUEAXH_ERR_OK;
    }

    const int operand_bytes = (opcode == 0x6C) ? 1 : (operand_size_override ? 2 : 4);
    const int address_size = address_size_override ? 32 : 64;
    const std::uint16_t port = static_cast<std::uint16_t>(context->regs[CPUEAXH_X86_REG_RDX] & 0xFFFFu);
    if (port != state->expected_port) {
        return CPUEAXH_ERR_ARG;
    }

    std::uint64_t count = 1;
    if (rep_prefix == 0xF2 || rep_prefix == 0xF3) {
        count = (address_size == 32)
            ? static_cast<std::uint32_t>(context->regs[CPUEAXH_X86_REG_RCX] & 0xFFFFFFFFu)
            : context->regs[CPUEAXH_X86_REG_RCX];
    }

    std::uint64_t index = (address_size == 32)
        ? static_cast<std::uint32_t>(context->regs[CPUEAXH_X86_REG_RDI] & 0xFFFFFFFFu)
        : context->regs[CPUEAXH_X86_REG_RDI];
    const std::uint64_t step = static_cast<std::uint64_t>(operand_bytes);
    const bool decrement = (context->rflags & RFLAGS_DF) != 0;

    for (std::uint64_t iteration = 0; iteration < count; ++iteration) {
        if (iteration >= state->values.size()) {
            return CPUEAXH_ERR_ARG;
        }
        const cpueaxh_err err = write_manual_ins_value(engine, index, operand_bytes, state->values[static_cast<std::size_t>(iteration)]);
        if (err != CPUEAXH_ERR_OK) {
            return err;
        }
        index = decrement ? (index - step) : (index + step);
    }

    write_manual_ins_index(context, address_size, index);
    if (rep_prefix == 0xF2 || rep_prefix == 0xF3) {
        write_manual_ins_count(context, address_size, 0);
    }
    return CPUEAXH_ERR_OK;
}

inline bool run_public_ins_escape_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    std::uint16_t port,
    std::uint64_t initial_rdi,
    std::uint64_t initial_rcx,
    std::uint64_t initial_rflags,
    const std::vector<std::uint32_t>& values,
    std::uint64_t verify_address,
    const std::vector<std::uint8_t>& expected_bytes,
    std::uint64_t expected_rdi,
    std::uint64_t expected_rcx,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_reg16(engine, CPUEAXH_X86_REG_RDX, port) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_RDI, initial_rdi) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_RCX, initial_rcx) ||
            !write_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, initial_rflags)) {
            failure.case_name = name;
            failure.detail = "register override failed";
            break;
        }

        ManualInsEscapeState state{};
        state.values = values;
        state.expected_port = port;
        cpueaxh_escape_handle handle = 0;
        err = cpueaxh_escape_add(
            engine,
            &handle,
            CPUEAXH_ESCAPE_INSN_INS,
            reinterpret_cast<void*>(manual_ins_escape_callback),
            &state,
            kGuestCodeBase,
            kGuestCodeBase + static_cast<std::uint64_t>(code.size()) - 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "cpueaxh_escape_add failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        std::vector<std::uint8_t> actual_bytes(expected_bytes.size(), 0);
        err = cpueaxh_mem_read(engine, verify_address, actual_bytes.data(), actual_bytes.size());
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "cpueaxh_mem_read failed";
            break;
        }
        if (actual_bytes != expected_bytes) {
            failure.case_name = name;
            failure.detail = "stored bytes mismatch";
            break;
        }

        std::uint64_t actual_rdi = 0;
        std::uint64_t actual_rcx = 0;
        std::uint64_t actual_rip = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_RDI, actual_rdi) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RCX, actual_rcx) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RIP, actual_rip)) {
            failure.case_name = name;
            failure.detail = "register readback failed";
            break;
        }
        if (actual_rdi != expected_rdi) {
            failure.case_name = name;
            failure.detail = "rdi mismatch";
            break;
        }
        if (actual_rcx != expected_rcx) {
            failure.case_name = name;
            failure.detail = "rcx mismatch";
            break;
        }
        if (actual_rip != kGuestCodeBase + static_cast<std::uint64_t>(code.size())) {
            failure.case_name = name;
            failure.detail = "rip mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline cpueaxh_x86_xmm apply_expected_pinsrw_xmm(const cpueaxh_x86_xmm& input, std::uint16_t value, std::uint8_t imm8) {
    cpueaxh_x86_xmm result = input;
    const std::uint64_t shift = static_cast<std::uint64_t>(imm8 & 0x07u) * 16u;
    if (shift < 64u) {
        result.low = (result.low & ~(0xFFFFull << shift)) | (static_cast<std::uint64_t>(value) << shift);
    }
    else {
        const std::uint64_t high_shift = shift - 64u;
        result.high = (result.high & ~(0xFFFFull << high_shift)) | (static_cast<std::uint64_t>(value) << high_shift);
    }
    return result;
}

inline cpueaxh_x86_xmm apply_expected_pinsrb_xmm(const cpueaxh_x86_xmm& input, std::uint8_t value, std::uint8_t imm8) {
    cpueaxh_x86_xmm result = input;
    const std::uint64_t shift = static_cast<std::uint64_t>(imm8 & 0x0Fu) * 8u;
    if (shift < 64u) {
        result.low = (result.low & ~(0xFFull << shift)) | (static_cast<std::uint64_t>(value) << shift);
    }
    else {
        const std::uint64_t high_shift = shift - 64u;
        result.high = (result.high & ~(0xFFull << high_shift)) | (static_cast<std::uint64_t>(value) << high_shift);
    }
    return result;
}

inline cpueaxh_x86_xmm apply_expected_pinsrd_xmm(const cpueaxh_x86_xmm& input, std::uint32_t value, std::uint8_t imm8) {
    cpueaxh_x86_xmm result = input;
    const std::uint64_t shift = static_cast<std::uint64_t>(imm8 & 0x03u) * 32u;
    if (shift < 64u) {
        result.low = (result.low & ~(0xFFFFFFFFull << shift)) | (static_cast<std::uint64_t>(value) << shift);
    }
    else {
        const std::uint64_t high_shift = shift - 64u;
        result.high = (result.high & ~(0xFFFFFFFFull << high_shift)) | (static_cast<std::uint64_t>(value) << high_shift);
    }
    return result;
}

inline cpueaxh_x86_xmm apply_expected_pinsrq_xmm(const cpueaxh_x86_xmm& input, std::uint64_t value, std::uint8_t imm8) {
    cpueaxh_x86_xmm result = input;
    if ((imm8 & 0x01u) != 0) {
        result.high = value;
    }
    else {
        result.low = value;
    }
    return result;
}

inline cpueaxh_x86_xmm apply_expected_aesimc_xmm(const cpueaxh_x86_xmm& input) {
    alignas(16) std::uint64_t qwords[2] = { input.low, input.high };
    const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(qwords));
    const __m128i transformed = _mm_aesimc_si128(value);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(qwords), transformed);
    return cpueaxh_x86_xmm{ qwords[0], qwords[1] };
}

inline cpueaxh_x86_xmm apply_expected_aesdec_xmm(const cpueaxh_x86_xmm& state, const cpueaxh_x86_xmm& round_key, bool last_round) {
    alignas(16) std::uint64_t state_qwords[2] = { state.low, state.high };
    alignas(16) std::uint64_t key_qwords[2] = { round_key.low, round_key.high };
    const __m128i state_value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state_qwords));
    const __m128i key_value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key_qwords));
    const __m128i transformed = last_round
        ? _mm_aesdeclast_si128(state_value, key_value)
        : _mm_aesdec_si128(state_value, key_value);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(state_qwords), transformed);
    return cpueaxh_x86_xmm{ state_qwords[0], state_qwords[1] };
}

inline std::uint64_t apply_expected_pinsrw_mmx(std::uint64_t input, std::uint16_t value, std::uint8_t imm8) {
    const std::uint64_t shift = static_cast<std::uint64_t>(imm8 & 0x03u) * 16u;
    return (input & ~(0xFFFFull << shift)) | (static_cast<std::uint64_t>(value) << shift);
}

inline void pack_public_xmm_bytes(const cpueaxh_x86_xmm& value, std::uint8_t bytes[16]) {
    for (int index = 0; index < 8; ++index) {
        bytes[index] = static_cast<std::uint8_t>((value.low >> (index * 8)) & 0xFFu);
        bytes[index + 8] = static_cast<std::uint8_t>((value.high >> (index * 8)) & 0xFFu);
    }
}

inline cpueaxh_x86_xmm unpack_public_xmm_bytes(const std::uint8_t bytes[16]) {
    cpueaxh_x86_xmm value{};
    for (int index = 0; index < 8; ++index) {
        value.low |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
        value.high |= static_cast<std::uint64_t>(bytes[index + 8]) << (index * 8);
    }
    return value;
}

inline std::uint64_t expected_pextrb(const cpueaxh_x86_xmm& source, std::uint8_t imm8) {
    std::uint8_t source_bytes[16] = {};
    pack_public_xmm_bytes(source, source_bytes);
    return source_bytes[imm8 & 0x0Fu];
}

inline std::uint64_t expected_pextrd(const cpueaxh_x86_xmm& source, std::uint8_t imm8) {
    std::uint8_t source_bytes[16] = {};
    pack_public_xmm_bytes(source, source_bytes);
    const int base = static_cast<int>(imm8 & 0x03u) * 4;
    return static_cast<std::uint32_t>(source_bytes[base])
         | (static_cast<std::uint32_t>(source_bytes[base + 1]) << 8)
         | (static_cast<std::uint32_t>(source_bytes[base + 2]) << 16)
         | (static_cast<std::uint32_t>(source_bytes[base + 3]) << 24);
}

inline std::uint64_t expected_pextrq(const cpueaxh_x86_xmm& source, std::uint8_t imm8) {
    return ((imm8 & 0x01u) != 0) ? source.high : source.low;
}

inline std::uint64_t apply_expected_palignr_mmx(std::uint64_t src1, std::uint64_t src2, std::uint8_t imm8) {
    std::uint8_t src1_bytes[8] = {};
    std::uint8_t src2_bytes[8] = {};
    std::uint8_t concat_bytes[16] = {};
    std::uint8_t result_bytes[8] = {};
    for (int index = 0; index < 8; ++index) {
        src1_bytes[index] = static_cast<std::uint8_t>((src1 >> (index * 8)) & 0xFFu);
        src2_bytes[index] = static_cast<std::uint8_t>((src2 >> (index * 8)) & 0xFFu);
        concat_bytes[index] = src2_bytes[index];
        concat_bytes[index + 8] = src1_bytes[index];
    }
    for (int index = 0; index < 8; ++index) {
        const int source_index = static_cast<int>(imm8) + index;
        result_bytes[index] = (source_index >= 0 && source_index < 16) ? concat_bytes[source_index] : 0x00u;
    }

    std::uint64_t result = 0;
    for (int index = 0; index < 8; ++index) {
        result |= static_cast<std::uint64_t>(result_bytes[index]) << (index * 8);
    }
    return result;
}

inline cpueaxh_x86_xmm apply_expected_palignr_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2, std::uint8_t imm8) {
    std::uint8_t src1_bytes[16] = {};
    std::uint8_t src2_bytes[16] = {};
    std::uint8_t concat_bytes[32] = {};
    std::uint8_t result_bytes[16] = {};
    pack_public_xmm_bytes(src1, src1_bytes);
    pack_public_xmm_bytes(src2, src2_bytes);
    for (int index = 0; index < 16; ++index) {
        concat_bytes[index] = src2_bytes[index];
        concat_bytes[index + 16] = src1_bytes[index];
    }
    for (int index = 0; index < 16; ++index) {
        const int source_index = static_cast<int>(imm8) + index;
        result_bytes[index] = (source_index >= 0 && source_index < 32) ? concat_bytes[source_index] : 0x00u;
    }
    return unpack_public_xmm_bytes(result_bytes);
}

inline cpueaxh_x86_ymm apply_expected_palignr_ymm(const cpueaxh_x86_ymm& src1, const cpueaxh_x86_ymm& src2, std::uint8_t imm8) {
    cpueaxh_x86_ymm result{};
    result.lower = apply_expected_palignr_xmm(src1.lower, src2.lower, imm8);
    result.upper = apply_expected_palignr_xmm(src1.upper, src2.upper, imm8);
    return result;
}

inline std::uint32_t public_xmm_dword(const cpueaxh_x86_xmm& value, int lane) {
    std::uint8_t bytes[16] = {};
    pack_public_xmm_bytes(value, bytes);
    const int base = lane * 4;
    return static_cast<std::uint32_t>(bytes[base])
        | (static_cast<std::uint32_t>(bytes[base + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[base + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[base + 3]) << 24);
}

inline void set_public_xmm_dword(cpueaxh_x86_xmm* value, int lane, std::uint32_t dword) {
    std::uint8_t bytes[16] = {};
    pack_public_xmm_bytes(*value, bytes);
    const int base = lane * 4;
    bytes[base] = static_cast<std::uint8_t>(dword & 0xFFu);
    bytes[base + 1] = static_cast<std::uint8_t>((dword >> 8) & 0xFFu);
    bytes[base + 2] = static_cast<std::uint8_t>((dword >> 16) & 0xFFu);
    bytes[base + 3] = static_cast<std::uint8_t>((dword >> 24) & 0xFFu);
    *value = unpack_public_xmm_bytes(bytes);
}

inline cpueaxh_x86_xmm apply_expected_pcmpeqd_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2) {
    cpueaxh_x86_xmm result{};
    for (int lane = 0; lane < 4; ++lane) {
        set_public_xmm_dword(&result, lane, public_xmm_dword(src1, lane) == public_xmm_dword(src2, lane) ? 0xFFFFFFFFu : 0u);
    }
    return result;
}

inline cpueaxh_x86_xmm apply_expected_pcmpeqq_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2) {
    return cpueaxh_x86_xmm{
        src1.low == src2.low ? 0xFFFFFFFFFFFFFFFFull : 0ull,
        src1.high == src2.high ? 0xFFFFFFFFFFFFFFFFull : 0ull
    };
}

inline cpueaxh_x86_xmm apply_expected_xor_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2) {
    return cpueaxh_x86_xmm{ src1.low ^ src2.low, src1.high ^ src2.high };
}

inline cpueaxh_x86_xmm apply_expected_or_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2) {
    return cpueaxh_x86_xmm{ src1.low | src2.low, src1.high | src2.high };
}

inline cpueaxh_x86_xmm apply_expected_psllq_xmm(const cpueaxh_x86_xmm& src, std::uint8_t imm8) {
    if (imm8 > 63) {
        return cpueaxh_x86_xmm{};
    }
    return cpueaxh_x86_xmm{ src.low << imm8, src.high << imm8 };
}

inline cpueaxh_x86_xmm apply_expected_psrlq_xmm(const cpueaxh_x86_xmm& src, std::uint8_t imm8) {
    if (imm8 > 63) {
        return cpueaxh_x86_xmm{};
    }
    return cpueaxh_x86_xmm{ src.low >> imm8, src.high >> imm8 };
}

inline cpueaxh_x86_xmm apply_expected_psrldq_xmm(const cpueaxh_x86_xmm& src, std::uint8_t imm8) {
    std::uint8_t src_bytes[16] = {};
    std::uint8_t result_bytes[16] = {};
    pack_public_xmm_bytes(src, src_bytes);
    for (int index = 0; index < 16; ++index) {
        const int source_index = index + static_cast<int>(imm8);
        result_bytes[index] = source_index < 16 ? src_bytes[source_index] : 0;
    }
    return unpack_public_xmm_bytes(result_bytes);
}

inline cpueaxh_x86_xmm apply_expected_punpcklqdq_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2) {
    return cpueaxh_x86_xmm{ src1.low, src2.low };
}

inline cpueaxh_x86_xmm apply_expected_punpckhqdq_xmm(const cpueaxh_x86_xmm& src1, const cpueaxh_x86_xmm& src2) {
    return cpueaxh_x86_xmm{ src1.high, src2.high };
}

inline cpueaxh_x86_ymm make_expected_ymm128(const cpueaxh_x86_xmm& lower) {
    cpueaxh_x86_ymm result{};
    result.lower = lower;
    return result;
}

inline std::uint64_t expected_vptest_flags(std::uint64_t initial_flags, const cpueaxh_x86_xmm& lhs, const cpueaxh_x86_xmm& rhs) {
    std::uint64_t flags = initial_flags & ~kStatusMask;
    if (((lhs.low & rhs.low) == 0) && ((lhs.high & rhs.high) == 0)) {
        flags |= kFlagZF;
    }
    if ((((~lhs.low) & rhs.low) == 0) && (((~lhs.high) & rhs.high) == 0)) {
        flags |= kFlagCF;
    }
    return flags;
}

inline cpueaxh_x86_xmm get_zmm_lane(const cpueaxh_x86_zmm& value, int lane) {
    switch (lane) {
    case 0: return value.lower.lower;
    case 1: return value.lower.upper;
    case 2: return value.upper.lower;
    default: return value.upper.upper;
    }
}

inline void set_zmm_lane(cpueaxh_x86_zmm* value, int lane, const cpueaxh_x86_xmm& lane_value) {
    switch (lane) {
    case 0:
        value->lower.lower = lane_value;
        return;
    case 1:
        value->lower.upper = lane_value;
        return;
    case 2:
        value->upper.lower = lane_value;
        return;
    default:
        value->upper.upper = lane_value;
        return;
    }
}

inline void pack_public_zmm_bytes(const cpueaxh_x86_zmm& value, std::uint8_t bytes[64]) {
    pack_public_xmm_bytes(value.lower.lower, bytes);
    pack_public_xmm_bytes(value.lower.upper, bytes + 16);
    pack_public_xmm_bytes(value.upper.lower, bytes + 32);
    pack_public_xmm_bytes(value.upper.upper, bytes + 48);
}

inline cpueaxh_x86_zmm unpack_public_zmm_bytes(const std::uint8_t bytes[64]) {
    cpueaxh_x86_zmm value{};
    value.lower.lower = unpack_public_xmm_bytes(bytes);
    value.lower.upper = unpack_public_xmm_bytes(bytes + 16);
    value.upper.lower = unpack_public_xmm_bytes(bytes + 32);
    value.upper.upper = unpack_public_xmm_bytes(bytes + 48);
    return value;
}

inline cpueaxh_x86_zmm apply_expected_palignr_zmm(const cpueaxh_x86_zmm& src1, const cpueaxh_x86_zmm& src2, int vector_bits, std::uint8_t imm8) {
    cpueaxh_x86_zmm result{};
    const int lane_count = vector_bits / 128;
    for (int lane = 0; lane < lane_count; ++lane) {
        set_zmm_lane(&result, lane, apply_expected_palignr_xmm(get_zmm_lane(src1, lane), get_zmm_lane(src2, lane), imm8));
    }
    return result;
}

inline cpueaxh_x86_zmm apply_expected_evex_palignr(
    const cpueaxh_x86_zmm& initial_dest,
    const cpueaxh_x86_zmm& src1,
    const cpueaxh_x86_zmm& src2,
    int vector_bits,
    std::uint8_t imm8,
    std::uint64_t writemask,
    bool has_writemask,
    bool zeroing) {
    const cpueaxh_x86_zmm computed = apply_expected_palignr_zmm(src1, src2, vector_bits, imm8);
    std::uint8_t dest_bytes[64] = {};
    std::uint8_t computed_bytes[64] = {};
    std::uint8_t result_bytes[64] = {};
    pack_public_zmm_bytes(initial_dest, dest_bytes);
    pack_public_zmm_bytes(computed, computed_bytes);
    const int active_bytes = vector_bits / 8;
    for (int index = 0; index < active_bytes; ++index) {
        const bool write_lane = !has_writemask || (((writemask >> index) & 0x1ull) != 0);
        result_bytes[index] = write_lane ? computed_bytes[index] : (zeroing ? 0x00u : dest_bytes[index]);
    }
    for (int index = active_bytes; index < 64; ++index) {
        result_bytes[index] = 0x00u;
    }
    return unpack_public_zmm_bytes(result_bytes);
}

inline std::uint64_t popcnt_source_mask(int operand_size) {
    switch (operand_size) {
    case 16:
        return 0xFFFFull;
    case 32:
        return 0xFFFFFFFFull;
    default:
        return 0xFFFFFFFFFFFFFFFFull;
    }
}

inline std::uint64_t count_bits_reference(std::uint64_t value, int operand_size) {
    std::uint64_t count = 0;
    const std::uint64_t masked = value & popcnt_source_mask(operand_size);
    for (int bit = 0; bit < operand_size; ++bit) {
        count += (masked >> bit) & 1ull;
    }
    return count;
}

inline std::uint64_t apply_expected_popcnt_dest(std::uint64_t initial_dest, std::uint64_t result, int operand_size) {
    switch (operand_size) {
    case 16:
        return (initial_dest & ~0xFFFFull) | (result & 0xFFFFull);
    case 32:
        return static_cast<std::uint32_t>(result);
    default:
        return result;
    }
}

inline bool run_public_popcnt_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    int source_reg,
    std::uint64_t source_value,
    int operand_size,
    std::uint64_t source_mem_address,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        if (source_reg >= 0) {
            if (!write_engine_reg(engine, source_reg, source_value)) {
                failure.case_name = name;
                failure.detail = "source register initialization failed";
                break;
            }
        }
        else {
            std::uint8_t buffer[8] = {};
            for (int index = 0; index < (operand_size / 8); ++index) {
                buffer[index] = static_cast<std::uint8_t>((source_value >> (index * 8)) & 0xFFu);
            }
            if (cpueaxh_mem_write(engine, source_mem_address, buffer, static_cast<std::size_t>(operand_size / 8)) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "source memory initialization failed";
                break;
            }
        }

        std::uint64_t initial_dest = 0;
        if (!read_engine_reg(engine, dest_reg, initial_dest)) {
            failure.case_name = name;
            failure.detail = "destination pre-read failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        const std::uint64_t masked_source = source_value & popcnt_source_mask(operand_size);
        const std::uint64_t expected_result = count_bits_reference(source_value, operand_size);
        const std::uint64_t expected_dest = apply_expected_popcnt_dest(initial_dest, expected_result, operand_size);
        const std::uint64_t expected_flags = (initial.rflags & ~kStatusMask) | ((masked_source == 0) ? kFlagZF : 0ull);

        std::uint64_t actual_dest = 0;
        if (!read_engine_reg(engine, dest_reg, actual_dest)) {
            failure.case_name = name;
            failure.detail = "destination readback failed";
            break;
        }
        if (actual_dest != expected_dest) {
            failure.case_name = name;
            failure.detail = "destination result mismatch";
            break;
        }

        std::uint64_t actual_flags = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, actual_flags)) {
            failure.case_name = name;
            failure.detail = "flags readback failed";
            break;
        }
        if ((actual_flags & kStatusMask) != (expected_flags & kStatusMask)) {
            failure.case_name = name;
            failure.detail = "flags mismatch";
            break;
        }

        if (source_reg >= 0) {
            std::uint64_t actual_source = 0;
            if (!read_engine_reg(engine, source_reg, actual_source)) {
                failure.case_name = name;
                failure.detail = "source readback failed";
                break;
            }
            if (actual_source != source_value) {
                failure.case_name = name;
                failure.detail = "source register changed unexpectedly";
                break;
            }
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_palignr_mmx_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    std::uint64_t dest_initial,
    int source_reg,
    std::uint64_t source_value,
    std::uint64_t expected_mm,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_reg(engine, dest_reg, dest_initial) ||
            !write_engine_reg(engine, source_reg, source_value)) {
            failure.case_name = name;
            failure.detail = "register initialization failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        std::uint64_t actual_mm = 0;
        if (!read_engine_reg(engine, dest_reg, actual_mm)) {
            failure.case_name = name;
            failure.detail = "mm readback failed";
            break;
        }
        if (actual_mm != expected_mm) {
            failure.case_name = name;
            failure.detail = "mm result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_palignr_vec_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    const cpueaxh_x86_ymm& dest_initial,
    int source1_reg,
    const cpueaxh_x86_ymm* source1_initial,
    int source2_reg,
    const cpueaxh_x86_ymm* source2_initial,
    const cpueaxh_x86_ymm* source2_memory_value,
    std::uint64_t source2_memory_address,
    int compare_bits,
    const cpueaxh_x86_ymm& expected_dest,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_ymm(engine, dest_reg, dest_initial)) {
            failure.case_name = name;
            failure.detail = "destination initialization failed";
            break;
        }
        if (source1_initial && !write_engine_ymm(engine, source1_reg, *source1_initial)) {
            failure.case_name = name;
            failure.detail = "source1 initialization failed";
            break;
        }
        if (source2_initial && !write_engine_ymm(engine, source2_reg, *source2_initial)) {
            failure.case_name = name;
            failure.detail = "source2 initialization failed";
            break;
        }
        if (source2_memory_value) {
            std::uint8_t bytes[32] = {};
            pack_public_xmm_bytes(source2_memory_value->lower, bytes);
            if (compare_bits == 256) {
                pack_public_xmm_bytes(source2_memory_value->upper, bytes + 16);
            }
            if (cpueaxh_mem_write(engine, source2_memory_address, bytes, static_cast<std::size_t>(compare_bits / 8)) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "source memory initialization failed";
                break;
            }
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        cpueaxh_x86_ymm actual{};
        if (!read_engine_ymm(engine, dest_reg, actual)) {
            failure.case_name = name;
            failure.detail = "ymm readback failed";
            break;
        }
        if (!equal_xmm(actual.lower, expected_dest.lower) ||
            !equal_xmm(actual.upper, expected_dest.upper)) {
            failure.case_name = name;
            failure.detail = "palignr result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_vex128_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    const cpueaxh_x86_ymm& dest_initial,
    int source1_reg,
    const cpueaxh_x86_ymm* source1_initial,
    int source2_reg,
    const cpueaxh_x86_ymm* source2_initial,
    const cpueaxh_x86_ymm& expected_dest,
    bool check_flags,
    std::uint64_t expected_flags,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_ymm(engine, dest_reg, dest_initial)) {
            failure.case_name = name;
            failure.detail = "destination initialization failed";
            break;
        }
        if (source1_initial && !write_engine_ymm(engine, source1_reg, *source1_initial)) {
            failure.case_name = name;
            failure.detail = "source1 initialization failed";
            break;
        }
        if (source2_initial && !write_engine_ymm(engine, source2_reg, *source2_initial)) {
            failure.case_name = name;
            failure.detail = "source2 initialization failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        cpueaxh_x86_ymm actual{};
        if (!read_engine_ymm(engine, dest_reg, actual)) {
            failure.case_name = name;
            failure.detail = "ymm readback failed";
            break;
        }
        if (!equal_ymm(actual, expected_dest)) {
            failure.case_name = name;
            failure.detail = "vex result mismatch";
            break;
        }
        if (check_flags) {
            std::uint64_t actual_flags = 0;
            if (!read_engine_reg(engine, CPUEAXH_X86_REG_EFLAGS, actual_flags)) {
                failure.case_name = name;
                failure.detail = "flags readback failed";
                break;
            }
            if ((actual_flags & kStatusMask) != (expected_flags & kStatusMask)) {
                failure.case_name = name;
                failure.detail = "vex flags mismatch";
                break;
            }
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_palignr_zmm_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    const cpueaxh_x86_zmm& dest_initial,
    int source1_reg,
    const cpueaxh_x86_zmm* source1_initial,
    int source2_reg,
    const cpueaxh_x86_zmm* source2_initial,
    const cpueaxh_x86_zmm* source2_memory_value,
    std::uint64_t source2_memory_address,
    int source2_memory_bits,
    int mask_reg,
    std::uint64_t mask_value,
    const cpueaxh_x86_zmm& expected_dest,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_zmm(engine, dest_reg, dest_initial)) {
            failure.case_name = name;
            failure.detail = "destination initialization failed";
            break;
        }
        if (source1_initial && !write_engine_zmm(engine, source1_reg, *source1_initial)) {
            failure.case_name = name;
            failure.detail = "source1 initialization failed";
            break;
        }
        if (source2_initial && !write_engine_zmm(engine, source2_reg, *source2_initial)) {
            failure.case_name = name;
            failure.detail = "source2 initialization failed";
            break;
        }
        if (mask_reg >= 0 && !write_engine_reg(engine, mask_reg, mask_value)) {
            failure.case_name = name;
            failure.detail = "writemask initialization failed";
            break;
        }
        if (source2_memory_value) {
            std::uint8_t bytes[64] = {};
            pack_public_zmm_bytes(*source2_memory_value, bytes);
            if (cpueaxh_mem_write(engine, source2_memory_address, bytes, static_cast<std::size_t>(source2_memory_bits / 8)) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "source memory initialization failed";
                break;
            }
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        cpueaxh_x86_zmm actual{};
        if (!read_engine_zmm(engine, dest_reg, actual)) {
            failure.case_name = name;
            failure.detail = "zmm readback failed";
            break;
        }
        if (!equal_zmm(actual, expected_dest)) {
            failure.case_name = name;
            failure.detail = "zmm result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_pinsrw_mmx_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int mm_reg,
    std::uint64_t initial_mm,
    int source_reg,
    std::uint64_t source_value,
    std::uint64_t expected_mm,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_reg(engine, mm_reg, initial_mm) ||
            !write_engine_reg(engine, source_reg, source_value)) {
            failure.case_name = name;
            failure.detail = "register initialization failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        std::uint64_t actual_mm = 0;
        if (!read_engine_reg(engine, mm_reg, actual_mm)) {
            failure.case_name = name;
            failure.detail = "mm readback failed";
            break;
        }
        if (actual_mm != expected_mm) {
            failure.case_name = name;
            failure.detail = "mm result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_pinsrw_xmm_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    const cpueaxh_x86_ymm& dest_initial,
    int source_reg,
    std::uint64_t source_value,
    int source1_reg,
    const cpueaxh_x86_ymm* source1_initial,
    const cpueaxh_x86_xmm& expected_lower,
    const cpueaxh_x86_xmm& expected_upper,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_ymm(engine, dest_reg, dest_initial) ||
            !write_engine_reg(engine, source_reg, source_value)) {
            failure.case_name = name;
            failure.detail = "register initialization failed";
            break;
        }
        if (source1_initial && (!write_engine_ymm(engine, source1_reg, *source1_initial))) {
            failure.case_name = name;
            failure.detail = "source1 initialization failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        cpueaxh_x86_ymm actual{};
        if (!read_engine_ymm(engine, dest_reg, actual)) {
            failure.case_name = name;
            failure.detail = "ymm readback failed";
            break;
        }
        if (!equal_xmm(actual.lower, expected_lower) || !equal_xmm(actual.upper, expected_upper)) {
            failure.case_name = name;
            failure.detail = "xmm result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_pinsr_xmm_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    const cpueaxh_x86_ymm& dest_initial,
    int source_reg,
    std::uint64_t source_value,
    int source_size,
    std::uint64_t source_memory_address,
    int source1_reg,
    const cpueaxh_x86_ymm* source1_initial,
    const cpueaxh_x86_xmm& expected_lower,
    const cpueaxh_x86_xmm& expected_upper,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_ymm(engine, dest_reg, dest_initial)) {
            failure.case_name = name;
            failure.detail = "destination initialization failed";
            break;
        }
        if (source_reg >= 0 && !write_engine_reg(engine, source_reg, source_value)) {
            failure.case_name = name;
            failure.detail = "source register initialization failed";
            break;
        }
        if (source_reg < 0 && source_size > 0) {
            std::uint8_t bytes[8] = {};
            for (int index = 0; index < source_size; ++index) {
                bytes[index] = static_cast<std::uint8_t>((source_value >> (index * 8)) & 0xFFu);
            }
            if (cpueaxh_mem_write(engine, source_memory_address, bytes, static_cast<std::size_t>(source_size)) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "source memory initialization failed";
                break;
            }
        }
        if (source1_initial && !write_engine_ymm(engine, source1_reg, *source1_initial)) {
            failure.case_name = name;
            failure.detail = "source1 initialization failed";
            break;
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        cpueaxh_x86_ymm actual{};
        if (!read_engine_ymm(engine, dest_reg, actual)) {
            failure.case_name = name;
            failure.detail = "ymm readback failed";
            break;
        }
        if (!equal_xmm(actual.lower, expected_lower) || !equal_xmm(actual.upper, expected_upper)) {
            failure.case_name = name;
            failure.detail = "pinsr xmm result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_pextr_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int source_reg,
    const cpueaxh_x86_xmm& source_value,
    int dest_reg,
    std::uint64_t initial_dest,
    std::uint64_t expected_dest,
    std::uint64_t memory_address,
    int memory_size,
    std::uint64_t expected_memory_value,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_xmm(engine, source_reg, source_value)) {
            failure.case_name = name;
            failure.detail = "source initialization failed";
            break;
        }
        if (dest_reg >= 0 && !write_engine_reg(engine, dest_reg, initial_dest)) {
            failure.case_name = name;
            failure.detail = "destination initialization failed";
            break;
        }
        if (memory_size > 0) {
            std::vector<std::uint8_t> initial_bytes(static_cast<std::size_t>(memory_size), 0xCDu);
            if (cpueaxh_mem_write(engine, memory_address, initial_bytes.data(), initial_bytes.size()) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "memory initialization failed";
                break;
            }
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        if (dest_reg >= 0) {
            std::uint64_t actual_dest = 0;
            if (!read_engine_reg(engine, dest_reg, actual_dest)) {
                failure.case_name = name;
                failure.detail = "destination readback failed";
                break;
            }
            if (actual_dest != expected_dest) {
                failure.case_name = name;
                failure.detail = "destination result mismatch";
                break;
            }
        }

        if (memory_size > 0) {
            std::vector<std::uint8_t> actual_bytes(static_cast<std::size_t>(memory_size), 0);
            if (cpueaxh_mem_read(engine, memory_address, actual_bytes.data(), actual_bytes.size()) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "memory readback failed";
                break;
            }

            std::uint64_t actual_value = 0;
            for (int index = 0; index < memory_size; ++index) {
                actual_value |= static_cast<std::uint64_t>(actual_bytes[static_cast<std::size_t>(index)]) << (index * 8);
            }
            if (actual_value != expected_memory_value) {
                failure.case_name = name;
                failure.detail = "memory result mismatch";
                break;
            }
        }

        cpueaxh_x86_xmm actual_source{};
        if (!read_engine_xmm(engine, source_reg, actual_source) || !equal_xmm(actual_source, source_value)) {
            failure.case_name = name;
            failure.detail = "source xmm changed unexpectedly";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool run_public_aesimc_case(
    const std::string& name,
    const std::vector<std::uint8_t>& code,
    std::uint64_t seed,
    int dest_reg,
    const cpueaxh_x86_ymm& dest_initial,
    int source_reg,
    const cpueaxh_x86_xmm* source_reg_value,
    const cpueaxh_x86_xmm* source_memory_value,
    std::uint64_t source_memory_address,
    const cpueaxh_x86_xmm& expected_lower,
    const cpueaxh_x86_xmm& expected_upper,
    Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }
        if (!write_engine_ymm(engine, dest_reg, dest_initial)) {
            failure.case_name = name;
            failure.detail = "destination initialization failed";
            break;
        }
        if (source_reg_value && !write_engine_xmm(engine, source_reg, *source_reg_value)) {
            failure.case_name = name;
            failure.detail = "source register initialization failed";
            break;
        }
        if (source_memory_value) {
            std::uint8_t bytes[16] = {};
            pack_public_xmm_bytes(*source_memory_value, bytes);
            if (cpueaxh_mem_write(engine, source_memory_address, bytes, sizeof(bytes)) != CPUEAXH_ERR_OK) {
                failure.case_name = name;
                failure.detail = "source memory initialization failed";
                break;
            }
        }

        err = cpueaxh_emu_start(engine, kGuestCodeBase, 0, 0, 1);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine));
            break;
        }

        cpueaxh_x86_ymm actual{};
        if (!read_engine_ymm(engine, dest_reg, actual)) {
            failure.case_name = name;
            failure.detail = "ymm readback failed";
            break;
        }
        if (!equal_xmm(actual.lower, expected_lower) || !equal_xmm(actual.upper, expected_upper)) {
            failure.case_name = name;
            failure.detail = "aes result mismatch";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

inline bool emit_host_mov_reg_imm64(std::vector<std::uint8_t>& code, std::uint8_t reg_low3, std::uint64_t imm);
inline bool run_host_stack_roundtrip_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_cached_rmw_recompute_case(const std::string& name, std::uint64_t seed, std::uint8_t opcode_kind, Failure& failure);
inline bool run_movs_source_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_rep_movs_source_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_rep_movs_dest_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_stack_source_exception_case(const std::string& name, const std::vector<std::uint8_t>& code, std::uint64_t seed, bool use_unmapped_rbp, Failure& failure);
inline bool run_stack_write_exception_case(const std::string& name, const std::vector<std::uint8_t>& code, std::uint64_t seed, Failure& failure);
inline bool run_xmm_store_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_maskmovdqu_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_x87_env_store_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_x87_env_load_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_fxsave_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_fxrstor_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_far_call_stack_split_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_multi_pop_stack_split_exception_case(const std::string& name, const std::vector<std::uint8_t>& code, std::uint64_t seed, Failure& failure);
inline bool run_enter_stack_write_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_enter_nested_frame_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_leave_frame_source_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_pop_memory_dest_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_pop_sreg_invalid_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);
inline bool run_noncanonical_control_transfer_exception_case(const std::string& name, const std::vector<std::uint8_t>& code, std::uint64_t seed, bool target_in_rax, Failure& failure);
inline bool run_iret_invalid_selector_exception_case(const std::string& name, std::uint64_t seed, Failure& failure);

} // namespace cpueaxh_test

