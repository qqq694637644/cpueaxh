#pragma once
// Split from test/demo/framework.hpp: manual case registry and manual-suite orchestration.
// Included through test/framework/framework.hpp; keep include order there.

#include "manual_helpers.hpp"

namespace cpueaxh_test {

inline std::uint64_t manual_special_case_count(const HostFeatures& features) {
    const std::uint64_t per_seed_special = (features.avx ? 57ull : 44ull) + (features.popcnt ? 3ull : 0ull) + 12ull + (features.rdpid ? 3ull : 0ull)
        + (features.aes ? 6ull : 0ull)
        + ((features.aes && features.avx) ? 2ull : 0ull)
        + 4ull;
    const std::uint64_t exception_special = 80ull + ((features.aes && features.avx) ? 2ull : 0ull);
    return kSeedCount * per_seed_special + kExceptionSeedCount * exception_special + kHostStackSeedCount * 4ull + kContextApiSeedCount;
}

inline bool run_manual_special_tests(const HostFeatures& features, std::uint64_t& executed, std::uint64_t total, Failure* first_failure = nullptr) {
    const std::vector<std::uint8_t> endbr64 = { 0xF3, 0x0F, 0x1E, 0xFA, 0xC3 };
    const std::vector<std::uint8_t> endbr32 = { 0xF3, 0x0F, 0x1E, 0xFB, 0xC3 };
    const std::vector<std::uint8_t> rcl_bx_cl = { 0x66, 0xD3, 0xD3 };
    const std::vector<std::uint8_t> rcr_r13w_0 = { 0x66, 0x41, 0xC1, 0xDD, 0x00 };
    const std::vector<std::uint8_t> shld_bx_cx_1 = { 0x66, 0x0F, 0xA4, 0xCB, 0x01 };
    const std::vector<std::uint8_t> shrd_r13w_r11w_cl = { 0x66, 0x45, 0x0F, 0xAD, 0xDD };
    const std::vector<std::uint8_t> rdsspq = { 0xF3, 0x48, 0x0F, 0x1E, 0xC8, 0xC3 };
    const std::vector<std::uint8_t> rdsspd = { 0xF3, 0x0F, 0x1E, 0xC8, 0xC3 };
    const std::vector<std::uint8_t> rdpid64 = { 0xF3, 0x48, 0x0F, 0xC7, 0xF8, 0xC3 };
    const std::vector<std::uint8_t> rdpid32 = { 0xF3, 0x0F, 0xC7, 0xF8, 0xC3 };
    const std::vector<std::uint8_t> rdpid_with_66 = { 0x66, 0xF3, 0x48, 0x0F, 0xC7, 0xF8, 0xC3 };
    const std::vector<std::uint8_t> setcc_ignored_modrm_reg = {
        0x0F, 0x94, 0x64, 0x24, 0x40, // setz byte ptr [rsp+0x40] with ModRM.reg=4; SETcc ignores reg bits
        0xC3
    };
    const std::vector<std::uint8_t> invalid_endbr_lock = { 0xF0, 0xF3, 0x0F, 0x1E, 0xFA };
    const std::vector<std::uint8_t> invalid_rdssp_lock = { 0xF0, 0xF3, 0x48, 0x0F, 0x1E, 0xC8 };
    const std::vector<std::uint8_t> invalid_rdpid_no_f3 = { 0x48, 0x0F, 0xC7, 0xF8 };
    const std::vector<std::uint8_t> xgetbv = { 0x0F, 0x01, 0xD0 };
    const std::vector<std::uint8_t> shl_unmapped = { 0x48, 0xC1, 0x24, 0x25, 0x00, 0x00, 0x40, 0x00, 0x01 };
    const std::vector<std::uint8_t> mul_unmapped = { 0x48, 0xF7, 0x24, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> imul_unmapped = { 0x48, 0x0F, 0xAF, 0x04, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> bsf_unmapped = { 0x48, 0x0F, 0xBC, 0x04, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> bt_unmapped = { 0x48, 0x0F, 0xBA, 0x24, 0x25, 0x00, 0x00, 0x40, 0x00, 0x00 };
    const std::vector<std::uint8_t> cmpxchg_unmapped = { 0x48, 0x0F, 0xB1, 0x04, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> xadd_unmapped = { 0x48, 0x0F, 0xC1, 0x04, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> push_unmapped = { 0x48, 0xFF, 0x34, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> call_unmapped = { 0x48, 0xFF, 0x14, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> jmp_unmapped = { 0x48, 0xFF, 0x24, 0x25, 0x00, 0x00, 0x40, 0x00 };
    const std::vector<std::uint8_t> jmp_rax = { 0x48, 0xFF, 0xE0 };
    const std::vector<std::uint8_t> call_rax = { 0x48, 0xFF, 0xD0 };
    const std::vector<std::uint8_t> pop_stack_unmapped = { 0x58 };
    const std::vector<std::uint8_t> ret_stack_unmapped = { 0xC3 };
    const std::vector<std::uint8_t> leave_stack_unmapped = { 0xC9 };
    const std::vector<std::uint8_t> push_stack_unmapped = { 0x50 };
    const std::vector<std::uint8_t> call_stack_unmapped = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
    const std::vector<std::uint8_t> fninit = { 0xDB, 0xE3 };
    const std::vector<std::uint8_t> fnstsw_ax_only = { 0xDF, 0xE0 };
    const std::vector<std::uint8_t> finit_only = { 0x9B, 0xDB, 0xE3 };
    const std::vector<std::uint8_t> fstsw_ax_only = { 0x9B, 0xDF, 0xE0 };
    const std::vector<std::uint8_t> fldcw_then_fnstcw = { 0xD9, 0x6C, 0x24, 0x40, 0xD9, 0x7C, 0x24, 0x42 };
    const std::vector<std::uint8_t> fldcw_then_fstcw = { 0xD9, 0x6C, 0x24, 0x40, 0x9B, 0xD9, 0x7C, 0x24, 0x42 };
    const std::vector<std::uint8_t> fldcw_only = { 0xD9, 0x6C, 0x24, 0x40 };
    const std::vector<std::uint8_t> fldenv_then_fnstcw = { 0xD9, 0x64, 0x24, 0x40, 0xD9, 0x7C, 0x24, 0x60 };
    const std::vector<std::uint8_t> fldenv_then_fstcw = { 0xD9, 0x64, 0x24, 0x40, 0x9B, 0xD9, 0x7C, 0x24, 0x60 };
    const std::vector<std::uint8_t> fldenv_only = { 0xD9, 0x64, 0x24, 0x40 };
    const std::vector<std::uint8_t> pinsrw_mmx_reg = { 0x0F, 0xC4, 0xC8, 0x02 };
    const std::vector<std::uint8_t> pinsrw_xmm_reg = { 0x66, 0x44, 0x0F, 0xC4, 0xC8, 0x05 };
    const std::vector<std::uint8_t> pinsrw_xmm0_ecx = { 0x66, 0x0F, 0xC4, 0xC1, 0x04 };
    const std::vector<std::uint8_t> vpinsrw_xmm_reg = { 0xC4, 0xE1, 0x69, 0xC4, 0xC8, 0x05 };
    const std::vector<std::uint8_t> evex_vpinsrw_xmm_reg = { 0x62, 0xF1, 0x6D, 0x08, 0xC4, 0xC8, 0x05 };
    const std::vector<std::uint8_t> invalid_evex_vpinsrw_ymm = { 0x62, 0xF1, 0x6D, 0x28, 0xC4, 0xC8, 0x05 };
    const std::vector<std::uint8_t> invalid_evex_vpinsrw_k1 = { 0x62, 0xF1, 0x6D, 0x09, 0xC4, 0xC8, 0x05 };
    const std::vector<std::uint8_t> pinsrb_xmm0_ecx = { 0x66, 0x0F, 0x3A, 0x20, 0xC1, 0x0D };
    const std::vector<std::uint8_t> pinsrd_xmm1_mem = { 0x66, 0x0F, 0x3A, 0x22, 0x4C, 0x24, 0x40, 0x02 };
    const std::vector<std::uint8_t> pinsrq_xmm2_rax = { 0x66, 0x48, 0x0F, 0x3A, 0x22, 0xD0, 0x01 };
    const std::vector<std::uint8_t> vpinsrb_xmm1_xmm2_rax = { 0xC4, 0xE3, 0x69, 0x20, 0xC8, 0x0E };
    const std::vector<std::uint8_t> vpinsrd_xmm1_xmm2_mem = { 0xC4, 0xE3, 0x69, 0x22, 0x4C, 0x24, 0x40, 0x03 };
    const std::vector<std::uint8_t> vpinsrq_xmm1_xmm2_rax = { 0xC4, 0xE3, 0xE9, 0x22, 0xC8, 0x01 };
    const std::vector<std::uint8_t> evex_vpinsrb_xmm1_xmm2_rax = { 0x62, 0xF3, 0x6D, 0x08, 0x20, 0xC8, 0x0C };
    const std::vector<std::uint8_t> evex_vpinsrd_xmm1_xmm2_mem = { 0x62, 0xF3, 0x6D, 0x08, 0x22, 0x4C, 0x24, 0x40, 0x01 };
    const std::vector<std::uint8_t> evex_vpinsrq_xmm1_xmm2_rax = { 0x62, 0xF3, 0xED, 0x08, 0x22, 0xC8, 0x01 };
    const std::vector<std::uint8_t> invalid_vpinsrb_l = { 0xC4, 0xE3, 0x6D, 0x20, 0xC8, 0x00 };
    const std::vector<std::uint8_t> invalid_evex_vpinsrb_ymm = { 0x62, 0xF3, 0x6D, 0x28, 0x20, 0xC8, 0x00 };
    const std::vector<std::uint8_t> invalid_evex_vpinsrd_k1 = { 0x62, 0xF3, 0x6D, 0x09, 0x22, 0xC8, 0x00 };
    const std::vector<std::uint8_t> aesimc_xmm1_xmm0 = { 0x66, 0x0F, 0x38, 0xDB, 0xC8 };
    const std::vector<std::uint8_t> aesimc_xmm1_mem = { 0x66, 0x0F, 0x38, 0xDB, 0x4C, 0x24, 0x40 };
    const std::vector<std::uint8_t> aesdec_xmm1_xmm0 = { 0x66, 0x0F, 0x38, 0xDE, 0xC8 };
    const std::vector<std::uint8_t> aesdec_xmm1_mem = { 0x66, 0x0F, 0x38, 0xDE, 0x4C, 0x24, 0x40 };
    const std::vector<std::uint8_t> aesdeclast_xmm1_xmm0 = { 0x66, 0x0F, 0x38, 0xDF, 0xC8 };
    const std::vector<std::uint8_t> aesdeclast_xmm1_mem = { 0x66, 0x0F, 0x38, 0xDF, 0x4C, 0x24, 0x40 };
    const std::vector<std::uint8_t> vaesimc_xmm1_xmm0 = { 0xC4, 0xE2, 0x79, 0xDB, 0xC8 };
    const std::vector<std::uint8_t> vaesimc_xmm1_mem = { 0xC4, 0xE2, 0x79, 0xDB, 0x4C, 0x24, 0x40 };
    const std::vector<std::uint8_t> invalid_vaesimc_vvvv = { 0xC4, 0xE2, 0x69, 0xDB, 0xC8 };
    const std::vector<std::uint8_t> pextrb_rax_xmm1 = { 0x66, 0x0F, 0x3A, 0x14, 0xC8, 0x0D };
    const std::vector<std::uint8_t> pextrd_rsp_xmm2 = { 0x66, 0x0F, 0x3A, 0x16, 0x54, 0x24, 0x40, 0x02 };
    const std::vector<std::uint8_t> pextrq_rax_xmm3 = { 0x66, 0x48, 0x0F, 0x3A, 0x16, 0xD8, 0x01 };
    const std::vector<std::uint8_t> vpextrb_rsp_xmm1 = { 0xC4, 0xE3, 0x79, 0x14, 0x4C, 0x24, 0x40, 0x0E };
    const std::vector<std::uint8_t> vpextrd_rax_xmm2 = { 0xC4, 0xE3, 0x79, 0x16, 0xD0, 0x03 };
    const std::vector<std::uint8_t> vpextrq_rsp_xmm3 = { 0xC4, 0xE3, 0xF9, 0x16, 0x5C, 0x24, 0x40, 0x01 };
    const std::vector<std::uint8_t> evex_vpextrb_rax_xmm1 = { 0x62, 0xF3, 0x05, 0x08, 0x14, 0xC8, 0x0D };
    const std::vector<std::uint8_t> evex_vpextrd_rsp_xmm2 = { 0x62, 0xF3, 0x05, 0x08, 0x16, 0x54, 0x24, 0x40, 0x02 };
    const std::vector<std::uint8_t> evex_vpextrq_rax_xmm3 = { 0x62, 0xF3, 0x85, 0x08, 0x16, 0xD8, 0x01 };
    const std::vector<std::uint8_t> invalid_vpextrb_l = { 0xC4, 0xE3, 0x7D, 0x14, 0xC8, 0x00 };
    const std::vector<std::uint8_t> invalid_evex_vpextrd_vvvv = { 0x62, 0xF3, 0x6D, 0x08, 0x16, 0xD0, 0x00 };
    const std::vector<std::uint8_t> palignr_mmx_zero = { 0x0F, 0x3A, 0x0F, 0xC8, 0x10 };
    const std::vector<std::uint8_t> palignr_xmm_reg = { 0x66, 0x0F, 0x3A, 0x0F, 0xC8, 0x05 };
    const std::vector<std::uint8_t> palignr_xmm_mem = { 0x66, 0x0F, 0x3A, 0x0F, 0x44, 0x24, 0x40, 0x07 };
    const std::vector<std::uint8_t> vpalignr_xmm_reg = { 0xC4, 0xE3, 0x69, 0x0F, 0xC8, 0x05 };
    const std::vector<std::uint8_t> vpalignr_ymm_reg = { 0xC4, 0xE3, 0x6D, 0x0F, 0xC8, 0x11 };
    const std::vector<std::uint8_t> evex_vpalignr_xmm_reg = { 0x62, 0xF3, 0x6D, 0x08, 0x0F, 0xC8, 0x05 };
    const std::vector<std::uint8_t> evex_vpalignr_ymm_reg = { 0x62, 0xF3, 0x6D, 0x28, 0x0F, 0xC8, 0x11 };
    const std::vector<std::uint8_t> evex_vpalignr_zmm_reg = { 0x62, 0xF3, 0x6D, 0x48, 0x0F, 0xC8, 0x11 };
    const std::vector<std::uint8_t> evex_vpalignr_zmm_k1_merge = { 0x62, 0xF3, 0x6D, 0x49, 0x0F, 0xC8, 0x11 };
    const std::vector<std::uint8_t> evex_vpalignr_zmm_k1_zero = { 0x62, 0xF3, 0x6D, 0xC9, 0x0F, 0xC8, 0x11 };
    const std::vector<std::uint8_t> popcnt_ax_mem = { 0x66, 0xF3, 0x0F, 0xB8, 0x44, 0x24, 0x40 };
    const std::vector<std::uint8_t> popcnt_eax_edx = { 0xF3, 0x0F, 0xB8, 0xC2 };
    const std::vector<std::uint8_t> popcnt_rax_rdx = { 0xF3, 0x48, 0x0F, 0xB8, 0xC2 };
    const std::vector<std::uint8_t> rep_insb_edi = { 0xF3, 0x67, 0x6C };
    const std::vector<std::uint8_t> insw_df_edi = { 0x67, 0x66, 0x6D };
    const std::vector<std::uint8_t> insd_rdi = { 0x6D };
    const std::vector<std::uint8_t> fnstenv_rsp = { 0xD9, 0x74, 0x24, 0x40 };
    const std::vector<std::uint8_t> fstenv_rsp = { 0x9B, 0xD9, 0x74, 0x24, 0x40 };
    const std::vector<std::uint8_t> fnstcw_rsp = { 0xD9, 0x7C, 0x24, 0x40, 0xC3 };
    const std::vector<std::uint8_t> fstcw_rsp = { 0x9B, 0xD9, 0x7C, 0x24, 0x40, 0xC3 };
    const std::vector<std::uint8_t> fninit_reset = { 0xDB, 0xE3, 0xD9, 0x7C, 0x24, 0x40, 0xDD, 0x7C, 0x24, 0x42 };
    const std::vector<std::uint8_t> finit_reset = { 0x9B, 0xDB, 0xE3, 0xD9, 0x7C, 0x24, 0x40, 0xDD, 0x7C, 0x24, 0x42 };
    const std::vector<std::uint8_t> fnstsw_rsp = { 0xDD, 0x7C, 0x24, 0x40 };
    const std::vector<std::uint8_t> fstsw_rsp = { 0x9B, 0xDD, 0x7C, 0x24, 0x40 };
    const std::vector<std::uint8_t> fnstsw_ax = { 0xDF, 0xE0 };
    const std::vector<std::uint8_t> fstsw_ax = { 0x9B, 0xDF, 0xE0 };
    const std::vector<std::uint8_t> stmxcsr_rsp = { 0x0F, 0xAE, 0x5C, 0x24, 0x40, 0xC3 };
    const std::vector<std::uint8_t> vstmxcsr_rsp = { 0xC5, 0xF8, 0xAE, 0x5C, 0x24, 0x40, 0xC3 };
    const std::vector<std::uint8_t> vmovq_xmm8_xmm1 = { 0xC5, 0x7A, 0x7E, 0xC1 };
    const std::vector<std::uint8_t> vmovaps_xmm8_xmm1 = { 0xC5, 0x78, 0x28, 0xC1 };
    const std::vector<std::uint8_t> vpcmpeqd_xmm8_xmm11_xmm9 = { 0xC4, 0x41, 0x21, 0x76, 0xC1 };
    const std::vector<std::uint8_t> vpcmpeqq_xmm8_xmm12_xmm9 = { 0xC4, 0x42, 0x19, 0x29, 0xC1 };
    const std::vector<std::uint8_t> vpxor_xmm8_xmm14_xmm1 = { 0xC5, 0x09, 0xEF, 0xC1 };
    const std::vector<std::uint8_t> vpor_xmm8_xmm12_xmm9 = { 0xC4, 0x41, 0x19, 0xEB, 0xC1 };
    const std::vector<std::uint8_t> vpsllq_xmm9_xmm12_5 = { 0xC4, 0xC1, 0x19, 0x73, 0xF1, 0x05 };
    const std::vector<std::uint8_t> vpsrlq_xmm9_xmm11_7 = { 0xC4, 0xC1, 0x21, 0x73, 0xD1, 0x07 };
    const std::vector<std::uint8_t> vpsrldq_xmm11_xmm9_8 = { 0xC4, 0xC1, 0x21, 0x73, 0xD9, 0x08 };
    const std::vector<std::uint8_t> vptest_xmm8_xmm9 = { 0xC4, 0x42, 0x79, 0x17, 0xC1 };
    const std::vector<std::uint8_t> vpunpckhqdq_xmm8_xmm14_xmm9 = { 0xC4, 0x41, 0x09, 0x6D, 0xC1 };
    const std::vector<std::uint8_t> vpunpcklqdq_xmm8_xmm14_xmm9 = { 0xC4, 0x41, 0x09, 0x6C, 0xC1 };
    const std::vector<std::uint8_t> invalid_popcnt_lock = { 0xF0, 0xF3, 0x48, 0x0F, 0xB8, 0xC2 };
    const std::vector<std::uint8_t> invalid_palignr_lock = { 0xF0, 0x66, 0x0F, 0x3A, 0x0F, 0xC8, 0x05 };
    const std::vector<std::uint8_t> invalid_palignr_misaligned = { 0x66, 0x0F, 0x3A, 0x0F, 0x44, 0x24, 0x41, 0x07 };

    bool all_ok = true;
    auto tick = [&](bool result, const Failure& failure) -> bool {
        if (!result) {
            all_ok = false;
            if (first_failure && first_failure->case_name.empty()) {
                *first_failure = failure;
            }
            std::cerr << "FAIL " << failure.case_name << std::endl;
            std::cerr << failure.detail << std::endl;
            // Allow CPUEAXH_TEST_CONTINUE=1 to surface every regression rather
            // than stopping at the first one. The suite still returns success
            // only when no failure was recorded (handled by run_all_tests).
            char* env_continue = nullptr;
            size_t env_continue_size = 0;
            const bool keep_going =
                _dupenv_s(&env_continue, &env_continue_size, "CPUEAXH_TEST_CONTINUE") == 0
                && env_continue != nullptr;
            if (env_continue) {
                free(env_continue);
            }
            if (keep_going) {
                ++executed;
                return true;
            }
            return false;
        }
        ++executed;
        if ((executed % 1024) == 0 || executed == total) {
            std::cout << "progress " << executed << "/" << total << std::endl;
        }
        return true;
    };

    for (std::uint64_t seed_index = 0; seed_index < kSeedCount; ++seed_index) {
        Failure failure;
        const std::uint64_t seed_compat_ret = seeded(seed_index, 0xE101);
        if (!tick(run_compat32_near_ret_case("compat32_ret:" + std::to_string(seed_compat_ret), seed_compat_ret, false, failure), failure)) return false;

        const std::uint64_t seed_compat_ret_imm = seeded(seed_index, 0xE102);
        if (!tick(run_compat32_near_ret_case("compat32_ret_imm:" + std::to_string(seed_compat_ret_imm), seed_compat_ret_imm, true, failure), failure)) return false;

        const std::uint64_t seed_compat_retf = seeded(seed_index, 0xE103);
        if (!tick(run_compat32_retf_to_64_case("compat32_retf64:" + std::to_string(seed_compat_retf), seed_compat_retf, failure), failure)) return false;

        const std::uint64_t seed_compat_jmp33 = seeded(seed_index, 0xE104);
        if (!tick(run_compat32_far_jmp_33_case("compat32_jmp33:" + std::to_string(seed_compat_jmp33), seed_compat_jmp33, failure), failure)) return false;

        const std::uint64_t seed0 = seeded(seed_index, 0xE001);
        if (!tick(run_manual_special_case("endbr64:" + std::to_string(seed0), endbr64, seed0, 0, false, failure), failure)) return false;

        const std::uint64_t seed_endbr32 = seeded(seed_index, 0xE011);
        if (!tick(run_manual_special_case("endbr32:" + std::to_string(seed_endbr32), endbr32, seed_endbr32, 0, false, failure), failure)) return false;

        const std::uint64_t seed_rcl_bx_cl = seeded(seed_index, 0xE070);
        const std::uint64_t rcl_bx_initial = 0x123456789ABC8001ull;
        const std::uint64_t rcl_flags = 0x202ull | kFlagCF | kFlagPF | kFlagAF | kFlagZF | kFlagSF;
        if (!tick(run_manual_rotate_carry_case(
            "rcl_bx_cl:" + std::to_string(seed_rcl_bx_cl),
            rcl_bx_cl,
            seed_rcl_bx_cl,
            CPUEAXH_X86_REG_RBX,
            CPUEAXH_X86_REG_RCX,
            rcl_bx_initial,
            1,
            rcl_flags,
            (rcl_bx_initial & ~0xFFFFull) | 0x0003ull,
            (rcl_flags & ~(kFlagCF | kFlagOF)) | kFlagCF | kFlagOF,
            failure), failure)) return false;

        const std::uint64_t seed_rcr_r13w_0 = seeded(seed_index, 0xE071);
        const std::uint64_t rcr_r13_initial = 0x1122334455668001ull;
        const std::uint64_t rcr_flags = 0x202ull | kFlagCF | kFlagPF | kFlagAF | kFlagZF | kFlagSF | kFlagOF;
        if (!tick(run_manual_rotate_carry_case(
            "rcr_r13w_0:" + std::to_string(seed_rcr_r13w_0),
            rcr_r13w_0,
            seed_rcr_r13w_0,
            CPUEAXH_X86_REG_R13,
            -1,
            rcr_r13_initial,
            0,
            rcr_flags,
            rcr_r13_initial,
            rcr_flags,
            failure), failure)) return false;

        const std::uint64_t seed_shld_bx_cx = seeded(seed_index, 0xE072);
        const std::uint64_t shld_bx_initial = 0x123456789ABC8001ull;
        const std::uint64_t shld_cx_initial = 0x2233445566774003ull;
        const std::uint64_t shld_flags = 0x202ull | kFlagPF | kFlagAF | kFlagZF | kFlagSF;
        if (!tick(run_manual_double_shift_case(
            "shld_bx_cx_1:" + std::to_string(seed_shld_bx_cx),
            shld_bx_cx_1,
            seed_shld_bx_cx,
            CPUEAXH_X86_REG_RBX,
            CPUEAXH_X86_REG_RCX,
            -1,
            shld_bx_initial,
            shld_cx_initial,
            0,
            shld_flags,
            (shld_bx_initial & ~0xFFFFull) | 0x0002ull,
            (shld_flags & ~(kFlagCF | kFlagPF | kFlagZF | kFlagSF | kFlagOF)) | kFlagAF | kFlagCF | kFlagOF,
            failure), failure)) return false;

        const std::uint64_t seed_shrd_r13_r11 = seeded(seed_index, 0xE073);
        const std::uint64_t shrd_r13_initial = 0x1122334455668001ull;
        const std::uint64_t shrd_r11_initial = 0x8877665544334002ull;
        const std::uint64_t shrd_flags = 0x202ull | kFlagAF | kFlagZF | kFlagSF | kFlagOF;
        if (!tick(run_manual_double_shift_case(
            "shrd_r13w_r11w_cl:" + std::to_string(seed_shrd_r13_r11),
            shrd_r13w_r11w_cl,
            seed_shrd_r13_r11,
            CPUEAXH_X86_REG_R13,
            CPUEAXH_X86_REG_R11,
            CPUEAXH_X86_REG_RCX,
            shrd_r13_initial,
            shrd_r11_initial,
            1,
            shrd_flags,
            (shrd_r13_initial & ~0xFFFFull) | 0x4000ull,
            (shrd_flags & ~(kFlagCF | kFlagPF | kFlagZF | kFlagSF | kFlagOF)) | kFlagAF | kFlagCF | kFlagPF | kFlagOF,
            failure), failure)) return false;

        const std::uint64_t seed1 = seeded(seed_index, 0xE002);
        if (!tick(run_manual_special_case("rdsspq:" + std::to_string(seed1), rdsspq, seed1, 0, false, failure), failure)) return false;

        const std::uint64_t seed_rdsspd = seeded(seed_index, 0xE012);
        if (!tick(run_manual_special_case("rdsspd:" + std::to_string(seed_rdsspd), rdsspd, seed_rdsspd, 0, false, failure), failure)) return false;

        if (features.rdpid) {
            const std::uint64_t seed2 = seeded(seed_index, 0xE003);
            const std::uint32_t processor_id = static_cast<std::uint32_t>(seeded(seed2, 0x90));
            if (!tick(run_manual_special_case("rdpid64:" + std::to_string(seed2), rdpid64, seed2, processor_id, true, failure), failure)) return false;

            const std::uint64_t seed_rdpid32 = seeded(seed_index, 0xE013);
            const std::uint32_t processor_id32 = static_cast<std::uint32_t>(seeded(seed_rdpid32, 0x91));
            if (!tick(run_manual_special_case("rdpid32:" + std::to_string(seed_rdpid32), rdpid32, seed_rdpid32, processor_id32, true, failure), failure)) return false;

            const std::uint64_t seed_rdpid66 = seeded(seed_index, 0xE017);
            const std::uint32_t processor_id66 = static_cast<std::uint32_t>(seeded(seed_rdpid66, 0x92));
            if (!tick(run_manual_special_case("rdpid66:" + std::to_string(seed_rdpid66), rdpid_with_66, seed_rdpid66, processor_id66, true, failure), failure)) return false;
        }


        const std::uint64_t seed_setcc = seeded(seed_index, 0xE016);
        if (!tick(run_manual_special_case("setcc_ignored_modrm_reg:" + std::to_string(seed_setcc), setcc_ignored_modrm_reg, seed_setcc, 0, false, failure), failure)) return false;

        const std::uint64_t seed_fnstcw = seeded(seed_index, 0xE014);
        if (!tick(run_manual_store_case("fnstcw:" + std::to_string(seed_fnstcw), fnstcw_rsp, seed_fnstcw, static_cast<std::uint32_t>(kInitialMxcsr), 0x40, 2, kInitialX87ControlWord, failure), failure)) return false;

        const std::uint64_t seed_fstcw = seeded(seed_index, 0xE015);
        if (!tick(run_manual_store_case("fstcw:" + std::to_string(seed_fstcw), fstcw_rsp, seed_fstcw, static_cast<std::uint32_t>(kInitialMxcsr), 0x40, 2, kInitialX87ControlWord, failure), failure)) return false;

        const std::uint64_t seed_fninit = seeded(seed_index, 0xE018);
        if (!tick(run_manual_x87_reset_case("fninit:" + std::to_string(seed_fninit), fninit_reset, seed_fninit, failure), failure)) return false;

        const std::uint64_t seed_finit = seeded(seed_index, 0xE019);
        if (!tick(run_manual_x87_reset_case("finit:" + std::to_string(seed_finit), finit_reset, seed_finit, failure), failure)) return false;

        const std::uint64_t seed_fnstsw_mem = seeded(seed_index, 0xE01A);
        const std::uint16_t fnstsw_mem_value = static_cast<std::uint16_t>(seeded(seed_fnstsw_mem, 0x94) & 0xFFFFu);
        if (!tick(run_manual_x87_status_case("fnstsw_mem:" + std::to_string(seed_fnstsw_mem), fnstsw_rsp, seed_fnstsw_mem, fnstsw_mem_value, false, 0x40, failure), failure)) return false;

        const std::uint64_t seed_fstsw_mem = seeded(seed_index, 0xE01B);
        const std::uint16_t fstsw_mem_value = static_cast<std::uint16_t>(seeded(seed_fstsw_mem, 0x95) & 0xFFFFu);
        if (!tick(run_manual_x87_status_case("fstsw_mem:" + std::to_string(seed_fstsw_mem), fstsw_rsp, seed_fstsw_mem, fstsw_mem_value, false, 0x40, failure), failure)) return false;

        const std::uint64_t seed_fnstsw_ax = seeded(seed_index, 0xE01C);
        const std::uint16_t fnstsw_ax_value = static_cast<std::uint16_t>(seeded(seed_fnstsw_ax, 0x96) & 0xFFFFu);
        if (!tick(run_manual_x87_status_case("fnstsw_ax:" + std::to_string(seed_fnstsw_ax), fnstsw_ax, seed_fnstsw_ax, fnstsw_ax_value, true, 0, failure), failure)) return false;

        const std::uint64_t seed_fstsw_ax = seeded(seed_index, 0xE01D);
        const std::uint16_t fstsw_ax_value = static_cast<std::uint16_t>(seeded(seed_fstsw_ax, 0x97) & 0xFFFFu);
        if (!tick(run_manual_x87_status_case("fstsw_ax:" + std::to_string(seed_fstsw_ax), fstsw_ax, seed_fstsw_ax, fstsw_ax_value, true, 0, failure), failure)) return false;

        const std::uint64_t seed_fldcw = seeded(seed_index, 0xE01E);
        const std::uint16_t fldcw_value = static_cast<std::uint16_t>(seeded(seed_fldcw, 0x98) & 0xFFFFu);
        if (!tick(run_manual_x87_fldcw_load_case("fldcw:" + std::to_string(seed_fldcw), fldcw_then_fnstcw, seed_fldcw, fldcw_value, 0, false, failure), failure)) return false;

        const std::uint64_t seed_fldcw_unmask = seeded(seed_index, 0xE01F);
        if (!tick(run_manual_x87_fldcw_unmask_case("fldcw_unmask:" + std::to_string(seed_fldcw_unmask), fldcw_then_fstcw, seed_fldcw_unmask, 0x0000u, 0x0001u, failure), failure)) return false;

        const std::uint64_t seed_fnstenv = seeded(seed_index, 0xE026);
        if (!tick(run_manual_x87_env_store_case(
            "fnstenv:" + std::to_string(seed_fnstenv),
            fnstenv_rsp,
            seed_fnstenv,
            static_cast<std::uint16_t>(seeded(seed_fnstenv, 0x9A) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fnstenv, 0x9B) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fnstenv, 0x9C) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fnstenv, 0x9D) & 0xFFFFu),
            seeded(seed_fnstenv, 0x9E),
            seeded(seed_fnstenv, 0x9F),
            false,
            false,
            failure), failure)) return false;

        const std::uint64_t seed_fstenv = seeded(seed_index, 0xE027);
        if (!tick(run_manual_x87_env_store_case(
            "fstenv:" + std::to_string(seed_fstenv),
            fstenv_rsp,
            seed_fstenv,
            static_cast<std::uint16_t>(seeded(seed_fstenv, 0xA0) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fstenv, 0xA1) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fstenv, 0xA2) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fstenv, 0xA3) & 0xFFFFu),
            seeded(seed_fstenv, 0xA4),
            seeded(seed_fstenv, 0xA5),
            false,
            true,
            failure), failure)) return false;

        const std::uint64_t seed_fldenv = seeded(seed_index, 0xE029);
        const std::uint16_t fldenv_control_word = static_cast<std::uint16_t>(seeded(seed_fldenv, 0xA6) & 0xFFFFu);
        const std::uint16_t fldenv_status_word = static_cast<std::uint16_t>(seeded(seed_fldenv, 0xA7) & 0xFFFFu);
        if (!tick(run_manual_x87_env_load_case(
            "fldenv:" + std::to_string(seed_fldenv),
            fldenv_then_fnstcw,
            seed_fldenv,
            fldenv_control_word,
            fldenv_status_word,
            static_cast<std::uint16_t>(seeded(seed_fldenv, 0xA8) & 0xFFFFu),
            static_cast<std::uint16_t>(seeded(seed_fldenv, 0xA9) & 0xFFFFu),
            seeded(seed_fldenv, 0xAA),
            seeded(seed_fldenv, 0xAB),
            expected_x87_pending(fldenv_control_word, fldenv_status_word),
            false,
            failure), failure)) return false;

        const std::uint64_t seed_fldenv_unmask = seeded(seed_index, 0xE02A);
        if (!tick(run_manual_x87_env_load_case(
            "fldenv_unmask:" + std::to_string(seed_fldenv_unmask),
            fldenv_then_fstcw,
            seed_fldenv_unmask,
            0x0000u,
            0x0001u,
            0xFFFFu,
            0,
            0,
            0,
            true,
            true,
            failure), failure)) return false;

        const std::uint64_t seed_stmxcsr = seeded(seed_index, 0xE016);
        const std::uint32_t mxcsr_value = static_cast<std::uint32_t>(0x1F80u | (seeded(seed_stmxcsr, 0x92) & 0x1F3Fu));
        if (!tick(run_manual_store_case("stmxcsr:" + std::to_string(seed_stmxcsr), stmxcsr_rsp, seed_stmxcsr, mxcsr_value, 0x40, 4, mxcsr_value, failure), failure)) return false;

        const std::uint64_t seed_pinsrw_mmx = seeded(seed_index, 0xE018);
        const std::uint64_t pinsrw_mmx_initial = seeded(seed_pinsrw_mmx, 0xB0);
        const std::uint16_t pinsrw_mmx_source = static_cast<std::uint16_t>(seeded(seed_pinsrw_mmx, 0xB1) & 0xFFFFu);
        if (!tick(run_public_pinsrw_mmx_case(
            "pinsrw_mmx:" + std::to_string(seed_pinsrw_mmx),
            pinsrw_mmx_reg,
            seed_pinsrw_mmx,
            CPUEAXH_X86_REG_MM1,
            pinsrw_mmx_initial,
            CPUEAXH_X86_REG_RAX,
            pinsrw_mmx_source,
            apply_expected_pinsrw_mmx(pinsrw_mmx_initial, pinsrw_mmx_source, 0x02),
            failure), failure)) return false;

        const std::uint64_t seed_pinsrw_xmm = seeded(seed_index, 0xE019);
        const cpueaxh_x86_ymm pinsrw_xmm_initial = make_seeded_ymm(seed_pinsrw_xmm, 0xB2);
        const std::uint16_t pinsrw_xmm_source = static_cast<std::uint16_t>(seeded(seed_pinsrw_xmm, 0xB3) & 0xFFFFu);
        if (!tick(run_public_pinsrw_xmm_case(
            "pinsrw_xmm:" + std::to_string(seed_pinsrw_xmm),
            pinsrw_xmm0_ecx,
            seed_pinsrw_xmm,
            CPUEAXH_X86_REG_YMM0,
            pinsrw_xmm_initial,
            CPUEAXH_X86_REG_RCX,
            pinsrw_xmm_source,
            0,
            nullptr,
            apply_expected_pinsrw_xmm(pinsrw_xmm_initial.lower, pinsrw_xmm_source, 0x04),
            pinsrw_xmm_initial.upper,
            failure), failure)) return false;

        const std::uint64_t seed_vpinsrw = seeded(seed_index, 0xE01A);
        const cpueaxh_x86_ymm vpinsrw_dest_initial = make_seeded_ymm(seed_vpinsrw, 0xB4);
        const cpueaxh_x86_ymm vpinsrw_src1_initial = make_seeded_ymm(seed_vpinsrw, 0xB8);
        const std::uint16_t vpinsrw_source = static_cast<std::uint16_t>(seeded(seed_vpinsrw, 0xBC) & 0xFFFFu);
        if (!tick(run_public_pinsrw_xmm_case(
            "vpinsrw_xmm:" + std::to_string(seed_vpinsrw),
            vpinsrw_xmm_reg,
            seed_vpinsrw,
            CPUEAXH_X86_REG_YMM1,
            vpinsrw_dest_initial,
            CPUEAXH_X86_REG_RAX,
            vpinsrw_source,
            CPUEAXH_X86_REG_YMM2,
            &vpinsrw_src1_initial,
            apply_expected_pinsrw_xmm(vpinsrw_src1_initial.lower, vpinsrw_source, 0x05),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrw = seeded(seed_index, 0xE01B);
        const cpueaxh_x86_ymm evex_vpinsrw_dest_initial = make_seeded_ymm(seed_evex_vpinsrw, 0xC0);
        const cpueaxh_x86_ymm evex_vpinsrw_src1_initial = make_seeded_ymm(seed_evex_vpinsrw, 0xC4);
        const std::uint16_t evex_vpinsrw_source = static_cast<std::uint16_t>(seeded(seed_evex_vpinsrw, 0xC8) & 0xFFFFu);
        if (!tick(run_public_pinsrw_xmm_case(
            "evex_vpinsrw_xmm:" + std::to_string(seed_evex_vpinsrw),
            evex_vpinsrw_xmm_reg,
            seed_evex_vpinsrw,
            CPUEAXH_X86_REG_YMM1,
            evex_vpinsrw_dest_initial,
            CPUEAXH_X86_REG_RAX,
            evex_vpinsrw_source,
            CPUEAXH_X86_REG_YMM2,
            &evex_vpinsrw_src1_initial,
            apply_expected_pinsrw_xmm(evex_vpinsrw_src1_initial.lower, evex_vpinsrw_source, 0x05),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_pinsrb = seeded(seed_index, 0xE04D);
        const cpueaxh_x86_ymm pinsrb_dest_initial = make_seeded_ymm(seed_pinsrb, 0xD8);
        const std::uint64_t pinsrb_source_value = seeded(seed_pinsrb, 0xD9);
        if (!tick(run_public_pinsr_xmm_case(
            "pinsrb_xmm0_ecx:" + std::to_string(seed_pinsrb),
            pinsrb_xmm0_ecx,
            seed_pinsrb,
            CPUEAXH_X86_REG_YMM0,
            pinsrb_dest_initial,
            CPUEAXH_X86_REG_RCX,
            pinsrb_source_value,
            1,
            0,
            0,
            nullptr,
            apply_expected_pinsrb_xmm(pinsrb_dest_initial.lower, static_cast<std::uint8_t>(pinsrb_source_value & 0xFFu), 0x0D),
            pinsrb_dest_initial.upper,
            failure), failure)) return false;

        const std::uint64_t seed_pinsrd = seeded(seed_index, 0xE04E);
        const cpueaxh_x86_ymm pinsrd_dest_initial = make_seeded_ymm(seed_pinsrd, 0xDA);
        const std::uint32_t pinsrd_source_value = static_cast<std::uint32_t>(seeded(seed_pinsrd, 0xDB));
        if (!tick(run_public_pinsr_xmm_case(
            "pinsrd_xmm1_mem:" + std::to_string(seed_pinsrd),
            pinsrd_xmm1_mem,
            seed_pinsrd,
            CPUEAXH_X86_REG_YMM1,
            pinsrd_dest_initial,
            -1,
            pinsrd_source_value,
            4,
            kGuestStackBase + kInitialRspOffset + 0x40,
            0,
            nullptr,
            apply_expected_pinsrd_xmm(pinsrd_dest_initial.lower, pinsrd_source_value, 0x02),
            pinsrd_dest_initial.upper,
            failure), failure)) return false;

        const std::uint64_t seed_pinsrq = seeded(seed_index, 0xE04F);
        const cpueaxh_x86_ymm pinsrq_dest_initial = make_seeded_ymm(seed_pinsrq, 0xDC);
        const std::uint64_t pinsrq_source_value = seeded(seed_pinsrq, 0xDD);
        if (!tick(run_public_pinsr_xmm_case(
            "pinsrq_xmm2_rax:" + std::to_string(seed_pinsrq),
            pinsrq_xmm2_rax,
            seed_pinsrq,
            CPUEAXH_X86_REG_YMM2,
            pinsrq_dest_initial,
            CPUEAXH_X86_REG_RAX,
            pinsrq_source_value,
            8,
            0,
            0,
            nullptr,
            apply_expected_pinsrq_xmm(pinsrq_dest_initial.lower, pinsrq_source_value, 0x01),
            pinsrq_dest_initial.upper,
            failure), failure)) return false;

        const std::uint64_t seed_vpinsrb = seeded(seed_index, 0xE050);
        const cpueaxh_x86_ymm vpinsrb_dest_initial = make_seeded_ymm(seed_vpinsrb, 0xDE);
        const cpueaxh_x86_ymm vpinsrb_src1_initial = make_seeded_ymm(seed_vpinsrb, 0xDF);
        const std::uint64_t vpinsrb_source_value = seeded(seed_vpinsrb, 0xE0);
        if (!tick(run_public_pinsr_xmm_case(
            "vpinsrb_xmm1_xmm2_rax:" + std::to_string(seed_vpinsrb),
            vpinsrb_xmm1_xmm2_rax,
            seed_vpinsrb,
            CPUEAXH_X86_REG_YMM1,
            vpinsrb_dest_initial,
            CPUEAXH_X86_REG_RAX,
            vpinsrb_source_value,
            1,
            0,
            CPUEAXH_X86_REG_YMM2,
            &vpinsrb_src1_initial,
            apply_expected_pinsrb_xmm(vpinsrb_src1_initial.lower, static_cast<std::uint8_t>(vpinsrb_source_value & 0xFFu), 0x0E),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_vpinsrd = seeded(seed_index, 0xE051);
        const cpueaxh_x86_ymm vpinsrd_dest_initial = make_seeded_ymm(seed_vpinsrd, 0xE1);
        const cpueaxh_x86_ymm vpinsrd_src1_initial = make_seeded_ymm(seed_vpinsrd, 0xE2);
        const std::uint32_t vpinsrd_source_value = static_cast<std::uint32_t>(seeded(seed_vpinsrd, 0xE3));
        if (!tick(run_public_pinsr_xmm_case(
            "vpinsrd_xmm1_xmm2_mem:" + std::to_string(seed_vpinsrd),
            vpinsrd_xmm1_xmm2_mem,
            seed_vpinsrd,
            CPUEAXH_X86_REG_YMM1,
            vpinsrd_dest_initial,
            -1,
            vpinsrd_source_value,
            4,
            kGuestStackBase + kInitialRspOffset + 0x40,
            CPUEAXH_X86_REG_YMM2,
            &vpinsrd_src1_initial,
            apply_expected_pinsrd_xmm(vpinsrd_src1_initial.lower, vpinsrd_source_value, 0x03),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_vpinsrq = seeded(seed_index, 0xE052);
        const cpueaxh_x86_ymm vpinsrq_dest_initial = make_seeded_ymm(seed_vpinsrq, 0xE4);
        const cpueaxh_x86_ymm vpinsrq_src1_initial = make_seeded_ymm(seed_vpinsrq, 0xE5);
        const std::uint64_t vpinsrq_source_value = seeded(seed_vpinsrq, 0xE6);
        if (!tick(run_public_pinsr_xmm_case(
            "vpinsrq_xmm1_xmm2_rax:" + std::to_string(seed_vpinsrq),
            vpinsrq_xmm1_xmm2_rax,
            seed_vpinsrq,
            CPUEAXH_X86_REG_YMM1,
            vpinsrq_dest_initial,
            CPUEAXH_X86_REG_RAX,
            vpinsrq_source_value,
            8,
            0,
            CPUEAXH_X86_REG_YMM2,
            &vpinsrq_src1_initial,
            apply_expected_pinsrq_xmm(vpinsrq_src1_initial.lower, vpinsrq_source_value, 0x01),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrb = seeded(seed_index, 0xE053);
        const cpueaxh_x86_ymm evex_vpinsrb_dest_initial = make_seeded_ymm(seed_evex_vpinsrb, 0xE7);
        const cpueaxh_x86_ymm evex_vpinsrb_src1_initial = make_seeded_ymm(seed_evex_vpinsrb, 0xE8);
        const std::uint64_t evex_vpinsrb_source_value = seeded(seed_evex_vpinsrb, 0xE9);
        if (!tick(run_public_pinsr_xmm_case(
            "evex_vpinsrb_xmm1_xmm2_rax:" + std::to_string(seed_evex_vpinsrb),
            evex_vpinsrb_xmm1_xmm2_rax,
            seed_evex_vpinsrb,
            CPUEAXH_X86_REG_YMM1,
            evex_vpinsrb_dest_initial,
            CPUEAXH_X86_REG_RAX,
            evex_vpinsrb_source_value,
            1,
            0,
            CPUEAXH_X86_REG_YMM2,
            &evex_vpinsrb_src1_initial,
            apply_expected_pinsrb_xmm(evex_vpinsrb_src1_initial.lower, static_cast<std::uint8_t>(evex_vpinsrb_source_value & 0xFFu), 0x0C),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrd = seeded(seed_index, 0xE054);
        const cpueaxh_x86_ymm evex_vpinsrd_dest_initial = make_seeded_ymm(seed_evex_vpinsrd, 0xEA);
        const cpueaxh_x86_ymm evex_vpinsrd_src1_initial = make_seeded_ymm(seed_evex_vpinsrd, 0xEB);
        const std::uint32_t evex_vpinsrd_source_value = static_cast<std::uint32_t>(seeded(seed_evex_vpinsrd, 0xEC));
        if (!tick(run_public_pinsr_xmm_case(
            "evex_vpinsrd_xmm1_xmm2_mem:" + std::to_string(seed_evex_vpinsrd),
            evex_vpinsrd_xmm1_xmm2_mem,
            seed_evex_vpinsrd,
            CPUEAXH_X86_REG_YMM1,
            evex_vpinsrd_dest_initial,
            -1,
            evex_vpinsrd_source_value,
            4,
            kGuestStackBase + kInitialRspOffset + 0x40,
            CPUEAXH_X86_REG_YMM2,
            &evex_vpinsrd_src1_initial,
            apply_expected_pinsrd_xmm(evex_vpinsrd_src1_initial.lower, evex_vpinsrd_source_value, 0x01),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrq = seeded(seed_index, 0xE055);
        const cpueaxh_x86_ymm evex_vpinsrq_dest_initial = make_seeded_ymm(seed_evex_vpinsrq, 0xED);
        const cpueaxh_x86_ymm evex_vpinsrq_src1_initial = make_seeded_ymm(seed_evex_vpinsrq, 0xEE);
        const std::uint64_t evex_vpinsrq_source_value = seeded(seed_evex_vpinsrq, 0xEF);
        if (!tick(run_public_pinsr_xmm_case(
            "evex_vpinsrq_xmm1_xmm2_rax:" + std::to_string(seed_evex_vpinsrq),
            evex_vpinsrq_xmm1_xmm2_rax,
            seed_evex_vpinsrq,
            CPUEAXH_X86_REG_YMM1,
            evex_vpinsrq_dest_initial,
            CPUEAXH_X86_REG_RAX,
            evex_vpinsrq_source_value,
            8,
            0,
            CPUEAXH_X86_REG_YMM2,
            &evex_vpinsrq_src1_initial,
            apply_expected_pinsrq_xmm(evex_vpinsrq_src1_initial.lower, evex_vpinsrq_source_value, 0x01),
            cpueaxh_x86_xmm{},
            failure), failure)) return false;

        if (features.aes) {
            const std::uint64_t seed_aesimc_reg = seeded(seed_index, 0xE059);
            const cpueaxh_x86_ymm aesimc_reg_dest = make_seeded_ymm(seed_aesimc_reg, 0xF0);
            const cpueaxh_x86_xmm aesimc_reg_source = make_seeded_xmm(seed_aesimc_reg, 0xF1);
            if (!tick(run_public_aesimc_case(
                "aesimc_xmm1_xmm0:" + std::to_string(seed_aesimc_reg),
                aesimc_xmm1_xmm0,
                seed_aesimc_reg,
                CPUEAXH_X86_REG_YMM1,
                aesimc_reg_dest,
                CPUEAXH_X86_REG_XMM0,
                &aesimc_reg_source,
                nullptr,
                0,
                apply_expected_aesimc_xmm(aesimc_reg_source),
                aesimc_reg_dest.upper,
                failure), failure)) return false;

            const std::uint64_t seed_aesimc_mem = seeded(seed_index, 0xE05A);
            const cpueaxh_x86_ymm aesimc_mem_dest = make_seeded_ymm(seed_aesimc_mem, 0xF2);
            const cpueaxh_x86_xmm aesimc_mem_source = make_seeded_xmm(seed_aesimc_mem, 0xF3);
            if (!tick(run_public_aesimc_case(
                "aesimc_xmm1_mem:" + std::to_string(seed_aesimc_mem),
                aesimc_xmm1_mem,
                seed_aesimc_mem,
                CPUEAXH_X86_REG_YMM1,
                aesimc_mem_dest,
                -1,
                nullptr,
                &aesimc_mem_source,
                kGuestStackBase + kInitialRspOffset + 0x40,
                apply_expected_aesimc_xmm(aesimc_mem_source),
                aesimc_mem_dest.upper,
                failure), failure)) return false;

            const std::uint64_t seed_aesdec_reg = seeded(seed_index, 0xE05E);
            const cpueaxh_x86_ymm aesdec_reg_dest = make_seeded_ymm(seed_aesdec_reg, 0xF8);
            const cpueaxh_x86_xmm aesdec_reg_source = make_seeded_xmm(seed_aesdec_reg, 0xF9);
            if (!tick(run_public_aesimc_case(
                "aesdec_xmm1_xmm0:" + std::to_string(seed_aesdec_reg),
                aesdec_xmm1_xmm0,
                seed_aesdec_reg,
                CPUEAXH_X86_REG_YMM1,
                aesdec_reg_dest,
                CPUEAXH_X86_REG_XMM0,
                &aesdec_reg_source,
                nullptr,
                0,
                apply_expected_aesdec_xmm(aesdec_reg_dest.lower, aesdec_reg_source, false),
                aesdec_reg_dest.upper,
                failure), failure)) return false;

            const std::uint64_t seed_aesdec_mem = seeded(seed_index, 0xE05F);
            const cpueaxh_x86_ymm aesdec_mem_dest = make_seeded_ymm(seed_aesdec_mem, 0xFA);
            const cpueaxh_x86_xmm aesdec_mem_source = make_seeded_xmm(seed_aesdec_mem, 0xFB);
            if (!tick(run_public_aesimc_case(
                "aesdec_xmm1_mem:" + std::to_string(seed_aesdec_mem),
                aesdec_xmm1_mem,
                seed_aesdec_mem,
                CPUEAXH_X86_REG_YMM1,
                aesdec_mem_dest,
                -1,
                nullptr,
                &aesdec_mem_source,
                kGuestStackBase + kInitialRspOffset + 0x40,
                apply_expected_aesdec_xmm(aesdec_mem_dest.lower, aesdec_mem_source, false),
                aesdec_mem_dest.upper,
                failure), failure)) return false;

            const std::uint64_t seed_aesdeclast_reg = seeded(seed_index, 0xE060);
            const cpueaxh_x86_ymm aesdeclast_reg_dest = make_seeded_ymm(seed_aesdeclast_reg, 0xFC);
            const cpueaxh_x86_xmm aesdeclast_reg_source = make_seeded_xmm(seed_aesdeclast_reg, 0xFD);
            if (!tick(run_public_aesimc_case(
                "aesdeclast_xmm1_xmm0:" + std::to_string(seed_aesdeclast_reg),
                aesdeclast_xmm1_xmm0,
                seed_aesdeclast_reg,
                CPUEAXH_X86_REG_YMM1,
                aesdeclast_reg_dest,
                CPUEAXH_X86_REG_XMM0,
                &aesdeclast_reg_source,
                nullptr,
                0,
                apply_expected_aesdec_xmm(aesdeclast_reg_dest.lower, aesdeclast_reg_source, true),
                aesdeclast_reg_dest.upper,
                failure), failure)) return false;

            const std::uint64_t seed_aesdeclast_mem = seeded(seed_index, 0xE061);
            const cpueaxh_x86_ymm aesdeclast_mem_dest = make_seeded_ymm(seed_aesdeclast_mem, 0xFE);
            const cpueaxh_x86_xmm aesdeclast_mem_source = make_seeded_xmm(seed_aesdeclast_mem, 0xFF);
            if (!tick(run_public_aesimc_case(
                "aesdeclast_xmm1_mem:" + std::to_string(seed_aesdeclast_mem),
                aesdeclast_xmm1_mem,
                seed_aesdeclast_mem,
                CPUEAXH_X86_REG_YMM1,
                aesdeclast_mem_dest,
                -1,
                nullptr,
                &aesdeclast_mem_source,
                kGuestStackBase + kInitialRspOffset + 0x40,
                apply_expected_aesdec_xmm(aesdeclast_mem_dest.lower, aesdeclast_mem_source, true),
                aesdeclast_mem_dest.upper,
                failure), failure)) return false;
        }

        if (features.aes && features.avx) {
            const std::uint64_t seed_vaesimc_reg = seeded(seed_index, 0xE05B);
            const cpueaxh_x86_ymm vaesimc_reg_dest = make_seeded_ymm(seed_vaesimc_reg, 0xF4);
            const cpueaxh_x86_xmm vaesimc_reg_source = make_seeded_xmm(seed_vaesimc_reg, 0xF5);
            if (!tick(run_public_aesimc_case(
                "vaesimc_xmm1_xmm0:" + std::to_string(seed_vaesimc_reg),
                vaesimc_xmm1_xmm0,
                seed_vaesimc_reg,
                CPUEAXH_X86_REG_YMM1,
                vaesimc_reg_dest,
                CPUEAXH_X86_REG_XMM0,
                &vaesimc_reg_source,
                nullptr,
                0,
                apply_expected_aesimc_xmm(vaesimc_reg_source),
                cpueaxh_x86_xmm{},
                failure), failure)) return false;

            const std::uint64_t seed_vaesimc_mem = seeded(seed_index, 0xE05C);
            const cpueaxh_x86_ymm vaesimc_mem_dest = make_seeded_ymm(seed_vaesimc_mem, 0xF6);
            const cpueaxh_x86_xmm vaesimc_mem_source = make_seeded_xmm(seed_vaesimc_mem, 0xF7);
            if (!tick(run_public_aesimc_case(
                "vaesimc_xmm1_mem:" + std::to_string(seed_vaesimc_mem),
                vaesimc_xmm1_mem,
                seed_vaesimc_mem,
                CPUEAXH_X86_REG_YMM1,
                vaesimc_mem_dest,
                -1,
                nullptr,
                &vaesimc_mem_source,
                kGuestStackBase + kInitialRspOffset + 0x40,
                apply_expected_aesimc_xmm(vaesimc_mem_source),
                cpueaxh_x86_xmm{},
                failure), failure)) return false;
        }

        const std::uint64_t seed_pextrb = seeded(seed_index, 0xE042);
        const cpueaxh_x86_xmm pextrb_source = make_seeded_xmm(seed_pextrb, 0xC9);
        if (!tick(run_public_pextr_case(
            "pextrb_rax_xmm1:" + std::to_string(seed_pextrb),
            pextrb_rax_xmm1,
            seed_pextrb,
            CPUEAXH_X86_REG_XMM1,
            pextrb_source,
            CPUEAXH_X86_REG_RAX,
            seeded(seed_pextrb, 0xCA),
            expected_pextrb(pextrb_source, 0x0D),
            0,
            0,
            0,
            failure), failure)) return false;

        const std::uint64_t seed_pextrd = seeded(seed_index, 0xE043);
        const cpueaxh_x86_xmm pextrd_source = make_seeded_xmm(seed_pextrd, 0xCB);
        if (!tick(run_public_pextr_case(
            "pextrd_rsp_xmm2:" + std::to_string(seed_pextrd),
            pextrd_rsp_xmm2,
            seed_pextrd,
            CPUEAXH_X86_REG_XMM2,
            pextrd_source,
            -1,
            0,
            0,
            kGuestStackBase + kInitialRspOffset + 0x40,
            4,
            expected_pextrd(pextrd_source, 0x02),
            failure), failure)) return false;

        const std::uint64_t seed_pextrq = seeded(seed_index, 0xE044);
        const cpueaxh_x86_xmm pextrq_source = make_seeded_xmm(seed_pextrq, 0xCC);
        if (!tick(run_public_pextr_case(
            "pextrq_rax_xmm3:" + std::to_string(seed_pextrq),
            pextrq_rax_xmm3,
            seed_pextrq,
            CPUEAXH_X86_REG_XMM3,
            pextrq_source,
            CPUEAXH_X86_REG_RAX,
            seeded(seed_pextrq, 0xCD),
            expected_pextrq(pextrq_source, 0x01),
            0,
            0,
            0,
            failure), failure)) return false;

        const std::uint64_t seed_vpextrb = seeded(seed_index, 0xE045);
        const cpueaxh_x86_xmm vpextrb_source = make_seeded_xmm(seed_vpextrb, 0xCE);
        if (!tick(run_public_pextr_case(
            "vpextrb_rsp_xmm1:" + std::to_string(seed_vpextrb),
            vpextrb_rsp_xmm1,
            seed_vpextrb,
            CPUEAXH_X86_REG_XMM1,
            vpextrb_source,
            -1,
            0,
            0,
            kGuestStackBase + kInitialRspOffset + 0x40,
            1,
            expected_pextrb(vpextrb_source, 0x0E),
            failure), failure)) return false;

        const std::uint64_t seed_vpextrd = seeded(seed_index, 0xE046);
        const cpueaxh_x86_xmm vpextrd_source = make_seeded_xmm(seed_vpextrd, 0xCF);
        if (!tick(run_public_pextr_case(
            "vpextrd_rax_xmm2:" + std::to_string(seed_vpextrd),
            vpextrd_rax_xmm2,
            seed_vpextrd,
            CPUEAXH_X86_REG_XMM2,
            vpextrd_source,
            CPUEAXH_X86_REG_RAX,
            seeded(seed_vpextrd, 0xD0),
            expected_pextrd(vpextrd_source, 0x03),
            0,
            0,
            0,
            failure), failure)) return false;

        const std::uint64_t seed_vpextrq = seeded(seed_index, 0xE047);
        const cpueaxh_x86_xmm vpextrq_source = make_seeded_xmm(seed_vpextrq, 0xD1);
        if (!tick(run_public_pextr_case(
            "vpextrq_rsp_xmm3:" + std::to_string(seed_vpextrq),
            vpextrq_rsp_xmm3,
            seed_vpextrq,
            CPUEAXH_X86_REG_XMM3,
            vpextrq_source,
            -1,
            0,
            0,
            kGuestStackBase + kInitialRspOffset + 0x40,
            8,
            expected_pextrq(vpextrq_source, 0x01),
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpextrb = seeded(seed_index, 0xE048);
        const cpueaxh_x86_xmm evex_vpextrb_source = make_seeded_xmm(seed_evex_vpextrb, 0xD2);
        if (!tick(run_public_pextr_case(
            "evex_vpextrb_rax_xmm1:" + std::to_string(seed_evex_vpextrb),
            evex_vpextrb_rax_xmm1,
            seed_evex_vpextrb,
            CPUEAXH_X86_REG_XMM1,
            evex_vpextrb_source,
            CPUEAXH_X86_REG_RAX,
            seeded(seed_evex_vpextrb, 0xD3),
            expected_pextrb(evex_vpextrb_source, 0x0D),
            0,
            0,
            0,
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpextrd = seeded(seed_index, 0xE049);
        const cpueaxh_x86_xmm evex_vpextrd_source = make_seeded_xmm(seed_evex_vpextrd, 0xD4);
        if (!tick(run_public_pextr_case(
            "evex_vpextrd_rsp_xmm2:" + std::to_string(seed_evex_vpextrd),
            evex_vpextrd_rsp_xmm2,
            seed_evex_vpextrd,
            CPUEAXH_X86_REG_XMM2,
            evex_vpextrd_source,
            -1,
            0,
            0,
            kGuestStackBase + kInitialRspOffset + 0x40,
            4,
            expected_pextrd(evex_vpextrd_source, 0x02),
            failure), failure)) return false;

        const std::uint64_t seed_evex_vpextrq = seeded(seed_index, 0xE04A);
        const cpueaxh_x86_xmm evex_vpextrq_source = make_seeded_xmm(seed_evex_vpextrq, 0xD5);
        if (!tick(run_public_pextr_case(
            "evex_vpextrq_rax_xmm3:" + std::to_string(seed_evex_vpextrq),
            evex_vpextrq_rax_xmm3,
            seed_evex_vpextrq,
            CPUEAXH_X86_REG_XMM3,
            evex_vpextrq_source,
            CPUEAXH_X86_REG_RAX,
            seeded(seed_evex_vpextrq, 0xD6),
            expected_pextrq(evex_vpextrq_source, 0x01),
            0,
            0,
            0,
            failure), failure)) return false;

        const std::uint64_t seed_palignr_mmx = seeded(seed_index, 0xE034);
        const std::uint64_t palignr_mmx_dest = seeded(seed_palignr_mmx, 0xE0);
        const std::uint64_t palignr_mmx_src = seeded(seed_palignr_mmx, 0xE1);
        if (!tick(run_public_palignr_mmx_case(
            "palignr_mmx_zero:" + std::to_string(seed_palignr_mmx),
            palignr_mmx_zero,
            seed_palignr_mmx,
            CPUEAXH_X86_REG_MM1,
            palignr_mmx_dest,
            CPUEAXH_X86_REG_MM0,
            palignr_mmx_src,
            apply_expected_palignr_mmx(palignr_mmx_dest, palignr_mmx_src, 0x10),
            failure), failure)) return false;

        const std::uint64_t seed_palignr_xmm_reg = seeded(seed_index, 0xE035);
        const cpueaxh_x86_ymm palignr_xmm_dest = make_seeded_ymm(seed_palignr_xmm_reg, 0xE2);
        const cpueaxh_x86_ymm palignr_xmm_src = make_seeded_ymm(seed_palignr_xmm_reg, 0xE3);
        cpueaxh_x86_ymm palignr_xmm_expected = palignr_xmm_dest;
        palignr_xmm_expected.lower = apply_expected_palignr_xmm(palignr_xmm_dest.lower, palignr_xmm_src.lower, 0x05);
        if (!tick(run_public_palignr_vec_case(
            "palignr_xmm_reg:" + std::to_string(seed_palignr_xmm_reg),
            palignr_xmm_reg,
            seed_palignr_xmm_reg,
            CPUEAXH_X86_REG_YMM1,
            palignr_xmm_dest,
            -1,
            nullptr,
            CPUEAXH_X86_REG_YMM0,
            &palignr_xmm_src,
            nullptr,
            0,
            128,
            palignr_xmm_expected,
            failure), failure)) return false;

        const std::uint64_t seed_palignr_xmm_mem = seeded(seed_index, 0xE036);
        const cpueaxh_x86_ymm palignr_xmm_mem_dest = make_seeded_ymm(seed_palignr_xmm_mem, 0xE4);
        const cpueaxh_x86_ymm palignr_xmm_mem_src = make_seeded_ymm(seed_palignr_xmm_mem, 0xE5);
        cpueaxh_x86_ymm palignr_xmm_mem_expected = palignr_xmm_mem_dest;
        palignr_xmm_mem_expected.lower = apply_expected_palignr_xmm(palignr_xmm_mem_dest.lower, palignr_xmm_mem_src.lower, 0x07);
        if (!tick(run_public_palignr_vec_case(
            "palignr_xmm_mem:" + std::to_string(seed_palignr_xmm_mem),
            palignr_xmm_mem,
            seed_palignr_xmm_mem,
            CPUEAXH_X86_REG_YMM0,
            palignr_xmm_mem_dest,
            -1,
            nullptr,
            -1,
            nullptr,
            &palignr_xmm_mem_src,
            kGuestStackBase + kInitialRspOffset + 0x40,
            128,
            palignr_xmm_mem_expected,
            failure), failure)) return false;

        const std::uint64_t seed_vpalignr_xmm = seeded(seed_index, 0xE037);
        const cpueaxh_x86_ymm vpalignr_xmm_dest = make_seeded_ymm(seed_vpalignr_xmm, 0xE6);
        const cpueaxh_x86_ymm vpalignr_xmm_src1 = make_seeded_ymm(seed_vpalignr_xmm, 0xE7);
        const cpueaxh_x86_ymm vpalignr_xmm_src2 = make_seeded_ymm(seed_vpalignr_xmm, 0xE8);
        cpueaxh_x86_ymm vpalignr_xmm_expected{};
        vpalignr_xmm_expected.lower = apply_expected_palignr_xmm(vpalignr_xmm_src1.lower, vpalignr_xmm_src2.lower, 0x05);
        if (!tick(run_public_palignr_vec_case(
            "vpalignr_xmm:" + std::to_string(seed_vpalignr_xmm),
            vpalignr_xmm_reg,
            seed_vpalignr_xmm,
            CPUEAXH_X86_REG_YMM1,
            vpalignr_xmm_dest,
            CPUEAXH_X86_REG_YMM2,
            &vpalignr_xmm_src1,
            CPUEAXH_X86_REG_YMM0,
            &vpalignr_xmm_src2,
            nullptr,
            0,
            128,
            vpalignr_xmm_expected,
            failure), failure)) return false;

        const std::uint64_t seed_vpalignr_ymm = seeded(seed_index, 0xE038);
        const cpueaxh_x86_ymm vpalignr_ymm_dest = make_seeded_ymm(seed_vpalignr_ymm, 0xE9);
        const cpueaxh_x86_ymm vpalignr_ymm_src1 = make_seeded_ymm(seed_vpalignr_ymm, 0xEA);
        const cpueaxh_x86_ymm vpalignr_ymm_src2 = make_seeded_ymm(seed_vpalignr_ymm, 0xEB);
        const cpueaxh_x86_ymm vpalignr_ymm_expected = apply_expected_palignr_ymm(vpalignr_ymm_src1, vpalignr_ymm_src2, 0x11);
        if (!tick(run_public_palignr_vec_case(
            "vpalignr_ymm:" + std::to_string(seed_vpalignr_ymm),
            vpalignr_ymm_reg,
            seed_vpalignr_ymm,
            CPUEAXH_X86_REG_YMM1,
            vpalignr_ymm_dest,
            CPUEAXH_X86_REG_YMM2,
            &vpalignr_ymm_src1,
            CPUEAXH_X86_REG_YMM0,
            &vpalignr_ymm_src2,
            nullptr,
            0,
            256,
            vpalignr_ymm_expected,
            failure), failure)) return false;

        const std::uint64_t seed_evex_palignr_xmm = seeded(seed_index, 0xE039);
        const cpueaxh_x86_zmm evex_palignr_xmm_dest = make_seeded_zmm(seed_evex_palignr_xmm, 0xEC);
        const cpueaxh_x86_zmm evex_palignr_xmm_src1 = make_seeded_zmm(seed_evex_palignr_xmm, 0xF0);
        const cpueaxh_x86_zmm evex_palignr_xmm_src2 = make_seeded_zmm(seed_evex_palignr_xmm, 0xF4);
        const cpueaxh_x86_zmm evex_palignr_xmm_expected = apply_expected_evex_palignr(
            evex_palignr_xmm_dest, evex_palignr_xmm_src1, evex_palignr_xmm_src2, 128, 0x05, 0, false, false);
        if (!tick(run_public_palignr_zmm_case(
            "evex_vpalignr_xmm:" + std::to_string(seed_evex_palignr_xmm),
            evex_vpalignr_xmm_reg,
            seed_evex_palignr_xmm,
            CPUEAXH_X86_REG_ZMM1,
            evex_palignr_xmm_dest,
            CPUEAXH_X86_REG_ZMM2,
            &evex_palignr_xmm_src1,
            CPUEAXH_X86_REG_ZMM0,
            &evex_palignr_xmm_src2,
            nullptr,
            0,
            0,
            -1,
            0,
            evex_palignr_xmm_expected,
            failure), failure)) return false;

        const std::uint64_t seed_evex_palignr_ymm = seeded(seed_index, 0xE03A);
        const cpueaxh_x86_zmm evex_palignr_ymm_dest = make_seeded_zmm(seed_evex_palignr_ymm, 0xF8);
        const cpueaxh_x86_zmm evex_palignr_ymm_src1 = make_seeded_zmm(seed_evex_palignr_ymm, 0xFC);
        const cpueaxh_x86_zmm evex_palignr_ymm_src2 = make_seeded_zmm(seed_evex_palignr_ymm, 0x100);
        const cpueaxh_x86_zmm evex_palignr_ymm_expected = apply_expected_evex_palignr(
            evex_palignr_ymm_dest, evex_palignr_ymm_src1, evex_palignr_ymm_src2, 256, 0x11, 0, false, false);
        if (!tick(run_public_palignr_zmm_case(
            "evex_vpalignr_ymm:" + std::to_string(seed_evex_palignr_ymm),
            evex_vpalignr_ymm_reg,
            seed_evex_palignr_ymm,
            CPUEAXH_X86_REG_ZMM1,
            evex_palignr_ymm_dest,
            CPUEAXH_X86_REG_ZMM2,
            &evex_palignr_ymm_src1,
            CPUEAXH_X86_REG_ZMM0,
            &evex_palignr_ymm_src2,
            nullptr,
            0,
            0,
            -1,
            0,
            evex_palignr_ymm_expected,
            failure), failure)) return false;

        const std::uint64_t seed_evex_palignr_zmm = seeded(seed_index, 0xE03D);
        const cpueaxh_x86_zmm evex_palignr_zmm_dest = make_seeded_zmm(seed_evex_palignr_zmm, 0x104);
        const cpueaxh_x86_zmm evex_palignr_zmm_src1 = make_seeded_zmm(seed_evex_palignr_zmm, 0x108);
        const cpueaxh_x86_zmm evex_palignr_zmm_src2 = make_seeded_zmm(seed_evex_palignr_zmm, 0x10C);
        const cpueaxh_x86_zmm evex_palignr_zmm_expected = apply_expected_evex_palignr(
            evex_palignr_zmm_dest, evex_palignr_zmm_src1, evex_palignr_zmm_src2, 512, 0x11, 0, false, false);
        if (!tick(run_public_palignr_zmm_case(
            "evex_vpalignr_zmm:" + std::to_string(seed_evex_palignr_zmm),
            evex_vpalignr_zmm_reg,
            seed_evex_palignr_zmm,
            CPUEAXH_X86_REG_ZMM1,
            evex_palignr_zmm_dest,
            CPUEAXH_X86_REG_ZMM2,
            &evex_palignr_zmm_src1,
            CPUEAXH_X86_REG_ZMM0,
            &evex_palignr_zmm_src2,
            nullptr,
            0,
            0,
            -1,
            0,
            evex_palignr_zmm_expected,
            failure), failure)) return false;

        const std::uint64_t seed_evex_palignr_merge = seeded(seed_index, 0xE03E);
        const cpueaxh_x86_zmm evex_palignr_merge_dest = make_seeded_zmm(seed_evex_palignr_merge, 0x110);
        const cpueaxh_x86_zmm evex_palignr_merge_src1 = make_seeded_zmm(seed_evex_palignr_merge, 0x114);
        const cpueaxh_x86_zmm evex_palignr_merge_src2 = make_seeded_zmm(seed_evex_palignr_merge, 0x118);
        const std::uint64_t evex_palignr_merge_mask = 0xF0F00F0FF0F00F0Full;
        const cpueaxh_x86_zmm evex_palignr_merge_expected = apply_expected_evex_palignr(
            evex_palignr_merge_dest, evex_palignr_merge_src1, evex_palignr_merge_src2, 512, 0x11, evex_palignr_merge_mask, true, false);
        if (!tick(run_public_palignr_zmm_case(
            "evex_vpalignr_zmm_merge:" + std::to_string(seed_evex_palignr_merge),
            evex_vpalignr_zmm_k1_merge,
            seed_evex_palignr_merge,
            CPUEAXH_X86_REG_ZMM1,
            evex_palignr_merge_dest,
            CPUEAXH_X86_REG_ZMM2,
            &evex_palignr_merge_src1,
            CPUEAXH_X86_REG_ZMM0,
            &evex_palignr_merge_src2,
            nullptr,
            0,
            0,
            CPUEAXH_X86_REG_K1,
            evex_palignr_merge_mask,
            evex_palignr_merge_expected,
            failure), failure)) return false;

        const std::uint64_t seed_evex_palignr_zero = seeded(seed_index, 0xE03F);
        const cpueaxh_x86_zmm evex_palignr_zero_dest = make_seeded_zmm(seed_evex_palignr_zero, 0x11C);
        const cpueaxh_x86_zmm evex_palignr_zero_src1 = make_seeded_zmm(seed_evex_palignr_zero, 0x120);
        const cpueaxh_x86_zmm evex_palignr_zero_src2 = make_seeded_zmm(seed_evex_palignr_zero, 0x124);
        const std::uint64_t evex_palignr_zero_mask = 0x00FF00FF00FF00FFull;
        const cpueaxh_x86_zmm evex_palignr_zero_expected = apply_expected_evex_palignr(
            evex_palignr_zero_dest, evex_palignr_zero_src1, evex_palignr_zero_src2, 512, 0x11, evex_palignr_zero_mask, true, true);
        if (!tick(run_public_palignr_zmm_case(
            "evex_vpalignr_zmm_zero:" + std::to_string(seed_evex_palignr_zero),
            evex_vpalignr_zmm_k1_zero,
            seed_evex_palignr_zero,
            CPUEAXH_X86_REG_ZMM1,
            evex_palignr_zero_dest,
            CPUEAXH_X86_REG_ZMM2,
            &evex_palignr_zero_src1,
            CPUEAXH_X86_REG_ZMM0,
            &evex_palignr_zero_src2,
            nullptr,
            0,
            0,
            CPUEAXH_X86_REG_K1,
            evex_palignr_zero_mask,
            evex_palignr_zero_expected,
            failure), failure)) return false;

        if (features.popcnt) {
            const std::uint64_t seed_popcnt16 = seeded(seed_index, 0xE030);
            const std::uint16_t popcnt16_source = static_cast<std::uint16_t>(seeded(seed_popcnt16, 0xDA) & 0xFFFFu);
            if (!tick(run_public_popcnt_case(
                "popcnt_ax_mem:" + std::to_string(seed_popcnt16),
                popcnt_ax_mem,
                seed_popcnt16,
                CPUEAXH_X86_REG_RAX,
                -1,
                popcnt16_source,
                16,
                kGuestStackBase + kInitialRspOffset + 0x40,
                failure), failure)) return false;

            const std::uint64_t seed_popcnt32 = seeded(seed_index, 0xE031);
            const std::uint32_t popcnt32_source = static_cast<std::uint32_t>(seeded(seed_popcnt32, 0xDB) & 0xFFFFFFFFu);
            if (!tick(run_public_popcnt_case(
                "popcnt_eax_edx:" + std::to_string(seed_popcnt32),
                popcnt_eax_edx,
                seed_popcnt32,
                CPUEAXH_X86_REG_RAX,
                CPUEAXH_X86_REG_RDX,
                popcnt32_source,
                32,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_popcnt64 = seeded(seed_index, 0xE032);
            const std::uint64_t popcnt64_source = seeded(seed_popcnt64, 0xDC);
            if (!tick(run_public_popcnt_case(
                "popcnt_rax_rdx:" + std::to_string(seed_popcnt64),
                popcnt_rax_rdx,
                seed_popcnt64,
                CPUEAXH_X86_REG_RAX,
                CPUEAXH_X86_REG_RDX,
                popcnt64_source,
                64,
                0,
                failure), failure)) return false;
        }

        if (features.avx) {
            const std::uint64_t seed_vstmxcsr = seeded(seed_index, 0xE017);
            const std::uint32_t vex_mxcsr_value = static_cast<std::uint32_t>(0x1F80u | (seeded(seed_vstmxcsr, 0x93) & 0x1F3Fu));
            if (!tick(run_manual_store_case("vstmxcsr:" + std::to_string(seed_vstmxcsr), vstmxcsr_rsp, seed_vstmxcsr, vex_mxcsr_value, 0x40, 4, vex_mxcsr_value, failure), failure)) return false;

            const std::uint64_t seed_vex = seeded(seed_index, 0xE080);
            const cpueaxh_x86_ymm vex_dest = make_seeded_ymm(seed_vex, 0x120);
            const cpueaxh_x86_ymm vex_src1 = make_seeded_ymm(seed_vex, 0x124);
            const cpueaxh_x86_ymm vex_src2 = make_seeded_ymm(seed_vex, 0x128);
            cpueaxh_x86_ymm expected_vmovq{};
            expected_vmovq.lower.low = vex_src2.lower.low;
            if (!tick(run_public_vex128_case(
                "vmovq_xmm8_xmm1:" + std::to_string(seed_vex),
                vmovq_xmm8_xmm1,
                seed_vex,
                CPUEAXH_X86_REG_YMM8,
                vex_dest,
                -1,
                nullptr,
                CPUEAXH_X86_REG_YMM1,
                &vex_src2,
                expected_vmovq,
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vmovaps = seeded(seed_index, 0xE081);
            const cpueaxh_x86_ymm vmovaps_dest = make_seeded_ymm(seed_vmovaps, 0x12C);
            const cpueaxh_x86_ymm vmovaps_src = make_seeded_ymm(seed_vmovaps, 0x130);
            if (!tick(run_public_vex128_case(
                "vmovaps_xmm8_xmm1:" + std::to_string(seed_vmovaps),
                vmovaps_xmm8_xmm1,
                seed_vmovaps,
                CPUEAXH_X86_REG_YMM8,
                vmovaps_dest,
                -1,
                nullptr,
                CPUEAXH_X86_REG_YMM1,
                &vmovaps_src,
                make_expected_ymm128(vmovaps_src.lower),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpcmpeqd = seeded(seed_index, 0xE082);
            const cpueaxh_x86_ymm vpcmpeqd_dest = make_seeded_ymm(seed_vpcmpeqd, 0x134);
            const cpueaxh_x86_ymm vpcmpeqd_src1 = make_seeded_ymm(seed_vpcmpeqd, 0x138);
            const cpueaxh_x86_ymm vpcmpeqd_src2 = make_seeded_ymm(seed_vpcmpeqd, 0x13C);
            if (!tick(run_public_vex128_case(
                "vpcmpeqd_xmm8_xmm11_xmm9:" + std::to_string(seed_vpcmpeqd),
                vpcmpeqd_xmm8_xmm11_xmm9,
                seed_vpcmpeqd,
                CPUEAXH_X86_REG_YMM8,
                vpcmpeqd_dest,
                CPUEAXH_X86_REG_YMM11,
                &vpcmpeqd_src1,
                CPUEAXH_X86_REG_YMM9,
                &vpcmpeqd_src2,
                make_expected_ymm128(apply_expected_pcmpeqd_xmm(vpcmpeqd_src1.lower, vpcmpeqd_src2.lower)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpcmpeqq = seeded(seed_index, 0xE083);
            const cpueaxh_x86_ymm vpcmpeqq_dest = make_seeded_ymm(seed_vpcmpeqq, 0x140);
            const cpueaxh_x86_ymm vpcmpeqq_src1 = make_seeded_ymm(seed_vpcmpeqq, 0x144);
            const cpueaxh_x86_ymm vpcmpeqq_src2 = make_seeded_ymm(seed_vpcmpeqq, 0x148);
            if (!tick(run_public_vex128_case(
                "vpcmpeqq_xmm8_xmm12_xmm9:" + std::to_string(seed_vpcmpeqq),
                vpcmpeqq_xmm8_xmm12_xmm9,
                seed_vpcmpeqq,
                CPUEAXH_X86_REG_YMM8,
                vpcmpeqq_dest,
                CPUEAXH_X86_REG_YMM12,
                &vpcmpeqq_src1,
                CPUEAXH_X86_REG_YMM9,
                &vpcmpeqq_src2,
                make_expected_ymm128(apply_expected_pcmpeqq_xmm(vpcmpeqq_src1.lower, vpcmpeqq_src2.lower)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpxor = seeded(seed_index, 0xE084);
            const cpueaxh_x86_ymm vpxor_dest = make_seeded_ymm(seed_vpxor, 0x14C);
            const cpueaxh_x86_ymm vpxor_src1 = make_seeded_ymm(seed_vpxor, 0x150);
            const cpueaxh_x86_ymm vpxor_src2 = make_seeded_ymm(seed_vpxor, 0x154);
            if (!tick(run_public_vex128_case(
                "vpxor_xmm8_xmm14_xmm1:" + std::to_string(seed_vpxor),
                vpxor_xmm8_xmm14_xmm1,
                seed_vpxor,
                CPUEAXH_X86_REG_YMM8,
                vpxor_dest,
                CPUEAXH_X86_REG_YMM14,
                &vpxor_src1,
                CPUEAXH_X86_REG_YMM1,
                &vpxor_src2,
                make_expected_ymm128(apply_expected_xor_xmm(vpxor_src1.lower, vpxor_src2.lower)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpor = seeded(seed_index, 0xE085);
            const cpueaxh_x86_ymm vpor_dest = make_seeded_ymm(seed_vpor, 0x158);
            const cpueaxh_x86_ymm vpor_src1 = make_seeded_ymm(seed_vpor, 0x15C);
            const cpueaxh_x86_ymm vpor_src2 = make_seeded_ymm(seed_vpor, 0x160);
            if (!tick(run_public_vex128_case(
                "vpor_xmm8_xmm12_xmm9:" + std::to_string(seed_vpor),
                vpor_xmm8_xmm12_xmm9,
                seed_vpor,
                CPUEAXH_X86_REG_YMM8,
                vpor_dest,
                CPUEAXH_X86_REG_YMM12,
                &vpor_src1,
                CPUEAXH_X86_REG_YMM9,
                &vpor_src2,
                make_expected_ymm128(apply_expected_or_xmm(vpor_src1.lower, vpor_src2.lower)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpsllq = seeded(seed_index, 0xE086);
            const cpueaxh_x86_ymm vpsllq_dest = make_seeded_ymm(seed_vpsllq, 0x164);
            const cpueaxh_x86_ymm vpsllq_src = make_seeded_ymm(seed_vpsllq, 0x168);
            if (!tick(run_public_vex128_case(
                "vpsllq_xmm9_xmm12_5:" + std::to_string(seed_vpsllq),
                vpsllq_xmm9_xmm12_5,
                seed_vpsllq,
                CPUEAXH_X86_REG_YMM9,
                vpsllq_dest,
                CPUEAXH_X86_REG_YMM12,
                &vpsllq_src,
                -1,
                nullptr,
                make_expected_ymm128(apply_expected_psllq_xmm(vpsllq_src.lower, 5)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpsrlq = seeded(seed_index, 0xE087);
            const cpueaxh_x86_ymm vpsrlq_dest = make_seeded_ymm(seed_vpsrlq, 0x16C);
            const cpueaxh_x86_ymm vpsrlq_src = make_seeded_ymm(seed_vpsrlq, 0x170);
            if (!tick(run_public_vex128_case(
                "vpsrlq_xmm9_xmm11_7:" + std::to_string(seed_vpsrlq),
                vpsrlq_xmm9_xmm11_7,
                seed_vpsrlq,
                CPUEAXH_X86_REG_YMM9,
                vpsrlq_dest,
                CPUEAXH_X86_REG_YMM11,
                &vpsrlq_src,
                -1,
                nullptr,
                make_expected_ymm128(apply_expected_psrlq_xmm(vpsrlq_src.lower, 7)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpsrldq = seeded(seed_index, 0xE088);
            const cpueaxh_x86_ymm vpsrldq_dest = make_seeded_ymm(seed_vpsrldq, 0x174);
            const cpueaxh_x86_ymm vpsrldq_src = make_seeded_ymm(seed_vpsrldq, 0x178);
            if (!tick(run_public_vex128_case(
                "vpsrldq_xmm11_xmm9_8:" + std::to_string(seed_vpsrldq),
                vpsrldq_xmm11_xmm9_8,
                seed_vpsrldq,
                CPUEAXH_X86_REG_YMM11,
                vpsrldq_dest,
                -1,
                nullptr,
                CPUEAXH_X86_REG_YMM9,
                &vpsrldq_src,
                make_expected_ymm128(apply_expected_psrldq_xmm(vpsrldq_src.lower, 8)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vptest = seeded(seed_index, 0xE089);
            const cpueaxh_x86_ymm vptest_src1 = make_seeded_ymm(seed_vptest, 0x17C);
            const cpueaxh_x86_ymm vptest_src2 = make_seeded_ymm(seed_vptest, 0x180);
            const std::uint64_t vptest_initial_flags = make_initial_context(seed_vptest).rflags;
            if (!tick(run_public_vex128_case(
                "vptest_xmm8_xmm9:" + std::to_string(seed_vptest),
                vptest_xmm8_xmm9,
                seed_vptest,
                CPUEAXH_X86_REG_YMM8,
                vptest_src1,
                -1,
                nullptr,
                CPUEAXH_X86_REG_YMM9,
                &vptest_src2,
                vptest_src1,
                true,
                expected_vptest_flags(vptest_initial_flags, vptest_src1.lower, vptest_src2.lower),
                failure), failure)) return false;

            const std::uint64_t seed_vpunpck = seeded(seed_index, 0xE08A);
            const cpueaxh_x86_ymm vpunpck_dest = make_seeded_ymm(seed_vpunpck, 0x184);
            const cpueaxh_x86_ymm vpunpck_src1 = make_seeded_ymm(seed_vpunpck, 0x188);
            const cpueaxh_x86_ymm vpunpck_src2 = make_seeded_ymm(seed_vpunpck, 0x18C);
            if (!tick(run_public_vex128_case(
                "vpunpckhqdq_xmm8_xmm14_xmm9:" + std::to_string(seed_vpunpck),
                vpunpckhqdq_xmm8_xmm14_xmm9,
                seed_vpunpck,
                CPUEAXH_X86_REG_YMM8,
                vpunpck_dest,
                CPUEAXH_X86_REG_YMM14,
                &vpunpck_src1,
                CPUEAXH_X86_REG_YMM9,
                &vpunpck_src2,
                make_expected_ymm128(apply_expected_punpckhqdq_xmm(vpunpck_src1.lower, vpunpck_src2.lower)),
                false,
                0,
                failure), failure)) return false;

            const std::uint64_t seed_vpunpckl = seeded(seed_index, 0xE08B);
            const cpueaxh_x86_ymm vpunpckl_dest = make_seeded_ymm(seed_vpunpckl, 0x190);
            const cpueaxh_x86_ymm vpunpckl_src1 = make_seeded_ymm(seed_vpunpckl, 0x194);
            const cpueaxh_x86_ymm vpunpckl_src2 = make_seeded_ymm(seed_vpunpckl, 0x198);
            if (!tick(run_public_vex128_case(
                "vpunpcklqdq_xmm8_xmm14_xmm9:" + std::to_string(seed_vpunpckl),
                vpunpcklqdq_xmm8_xmm14_xmm9,
                seed_vpunpckl,
                CPUEAXH_X86_REG_YMM8,
                vpunpckl_dest,
                CPUEAXH_X86_REG_YMM14,
                &vpunpckl_src1,
                CPUEAXH_X86_REG_YMM9,
                &vpunpckl_src2,
                make_expected_ymm128(apply_expected_punpcklqdq_xmm(vpunpckl_src1.lower, vpunpckl_src2.lower)),
                false,
                0,
                failure), failure)) return false;
        }
    }

    for (std::uint64_t seed_index = 0; seed_index < kExceptionSeedCount; ++seed_index) {
        Failure failure;
        const std::uint64_t seed3 = seeded(seed_index, 0xE004);
        if (!tick(run_manual_exception_case_public("public_endbr_lock_ud:" + std::to_string(seed3), invalid_endbr_lock, seed3, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("endbr_lock_ud:" + std::to_string(seed3), invalid_endbr_lock, seed3, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed4 = seeded(seed_index, 0xE005);
        if (!tick(run_manual_exception_case_public("public_rdssp_lock_ud:" + std::to_string(seed4), invalid_rdssp_lock, seed4, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("rdssp_lock_ud:" + std::to_string(seed4), invalid_rdssp_lock, seed4, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed5 = seeded(seed_index, 0xE006);
        if (!tick(run_manual_exception_case_public("public_rdpid_no_f3_ud:" + std::to_string(seed5), invalid_rdpid_no_f3, seed5, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("rdpid_no_f3_ud:" + std::to_string(seed5), invalid_rdpid_no_f3, seed5, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_xgetbv_osxsave = seeded(seed_index, 0xE007);
        if (!tick(run_manual_public_cr4_exception_case("public_xgetbv_osxsave_ud:" + std::to_string(seed_xgetbv_osxsave), xgetbv, seed_xgetbv_osxsave, 0, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_cr4_exception_case("xgetbv_osxsave_ud:" + std::to_string(seed_xgetbv_osxsave), xgetbv, seed_xgetbv_osxsave, 0, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_shl_unmapped = seeded(seed_index, 0xE008);
        if (!tick(run_manual_exception_case("shl_unmapped_pf:" + std::to_string(seed_shl_unmapped), shl_unmapped, seed_shl_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_mul_unmapped = seeded(seed_index, 0xE009);
        if (!tick(run_manual_exception_case("mul_unmapped_pf:" + std::to_string(seed_mul_unmapped), mul_unmapped, seed_mul_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_imul_unmapped = seeded(seed_index, 0xE00A);
        if (!tick(run_manual_exception_case("imul_unmapped_pf:" + std::to_string(seed_imul_unmapped), imul_unmapped, seed_imul_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_bsf_unmapped = seeded(seed_index, 0xE00B);
        if (!tick(run_manual_exception_case("bsf_unmapped_pf:" + std::to_string(seed_bsf_unmapped), bsf_unmapped, seed_bsf_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_bt_unmapped = seeded(seed_index, 0xE00C);
        if (!tick(run_manual_exception_case("bt_unmapped_pf:" + std::to_string(seed_bt_unmapped), bt_unmapped, seed_bt_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_cmpxchg_unmapped = seeded(seed_index, 0xE00D);
        if (!tick(run_manual_exception_case("cmpxchg_unmapped_pf:" + std::to_string(seed_cmpxchg_unmapped), cmpxchg_unmapped, seed_cmpxchg_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_xadd_unmapped = seeded(seed_index, 0xE00E);
        if (!tick(run_manual_exception_case("xadd_unmapped_pf:" + std::to_string(seed_xadd_unmapped), xadd_unmapped, seed_xadd_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_push_unmapped = seeded(seed_index, 0xE00F);
        if (!tick(run_manual_exception_case("push_unmapped_pf:" + std::to_string(seed_push_unmapped), push_unmapped, seed_push_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_call_unmapped = seeded(seed_index, 0xE010);
        if (!tick(run_manual_exception_case("call_unmapped_pf:" + std::to_string(seed_call_unmapped), call_unmapped, seed_call_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_jmp_unmapped = seeded(seed_index, 0xE011);
        if (!tick(run_manual_exception_case("jmp_unmapped_pf:" + std::to_string(seed_jmp_unmapped), jmp_unmapped, seed_jmp_unmapped, CPUEAXH_EXCEPTION_PF, failure), failure)) return false;

        const std::uint64_t seed_jmp_noncanonical = seeded(seed_index, 0xE06D);
        if (!tick(run_noncanonical_control_transfer_exception_case("jmp_rax_noncanonical_gp:" + std::to_string(seed_jmp_noncanonical), jmp_rax, seed_jmp_noncanonical, true, failure), failure)) return false;

        const std::uint64_t seed_call_noncanonical = seeded(seed_index, 0xE06E);
        if (!tick(run_noncanonical_control_transfer_exception_case("call_rax_noncanonical_gp:" + std::to_string(seed_call_noncanonical), call_rax, seed_call_noncanonical, true, failure), failure)) return false;

        const std::uint64_t seed_ret_noncanonical = seeded(seed_index, 0xE06F);
        if (!tick(run_noncanonical_control_transfer_exception_case("ret_noncanonical_gp:" + std::to_string(seed_ret_noncanonical), ret_stack_unmapped, seed_ret_noncanonical, false, failure), failure)) return false;

        const std::uint64_t seed_iret_invalid_selector = seeded(seed_index, 0xE070);
        if (!tick(run_iret_invalid_selector_exception_case("iret_invalid_selector_gp:" + std::to_string(seed_iret_invalid_selector), seed_iret_invalid_selector, failure), failure)) return false;

        const std::uint64_t seed_movs_unmapped = seeded(seed_index, 0xE018);
        if (!tick(run_movs_source_exception_case("movsq_source_unmapped_pf:" + std::to_string(seed_movs_unmapped), seed_movs_unmapped, failure), failure)) return false;

        const std::uint64_t seed_rep_movs_split = seeded(seed_index, 0xE06B);
        if (!tick(run_rep_movs_source_split_exception_case("rep_movsq_source_split_pf:" + std::to_string(seed_rep_movs_split), seed_rep_movs_split, failure), failure)) return false;

        const std::uint64_t seed_rep_movs_dest_split = seeded(seed_index, 0xE06C);
        if (!tick(run_rep_movs_dest_split_exception_case("rep_movsq_dest_split_pf:" + std::to_string(seed_rep_movs_dest_split), seed_rep_movs_dest_split, failure), failure)) return false;

        const std::uint64_t seed_pop_stack_unmapped = seeded(seed_index, 0xE019);
        if (!tick(run_stack_source_exception_case("pop_stack_unmapped_pf:" + std::to_string(seed_pop_stack_unmapped), pop_stack_unmapped, seed_pop_stack_unmapped, false, failure), failure)) return false;

        const std::uint64_t seed_ret_stack_unmapped = seeded(seed_index, 0xE01A);
        if (!tick(run_stack_source_exception_case("ret_stack_unmapped_pf:" + std::to_string(seed_ret_stack_unmapped), ret_stack_unmapped, seed_ret_stack_unmapped, false, failure), failure)) return false;

        const std::uint64_t seed_leave_stack_unmapped = seeded(seed_index, 0xE01B);
        if (!tick(run_stack_source_exception_case("leave_stack_unmapped_pf:" + std::to_string(seed_leave_stack_unmapped), leave_stack_unmapped, seed_leave_stack_unmapped, true, failure), failure)) return false;

        const std::uint64_t seed_push_stack_unmapped = seeded(seed_index, 0xE01C);
        if (!tick(run_stack_write_exception_case("push_stack_unmapped_pf:" + std::to_string(seed_push_stack_unmapped), push_stack_unmapped, seed_push_stack_unmapped, failure), failure)) return false;

        const std::uint64_t seed_call_stack_unmapped = seeded(seed_index, 0xE01D);
        if (!tick(run_stack_write_exception_case("call_stack_unmapped_pf:" + std::to_string(seed_call_stack_unmapped), call_stack_unmapped, seed_call_stack_unmapped, failure), failure)) return false;

        const std::uint64_t seed_xmm_split_store = seeded(seed_index, 0xE01E);
        if (!tick(run_xmm_store_split_exception_case("xmm_store_split_pf:" + std::to_string(seed_xmm_split_store), seed_xmm_split_store, failure), failure)) return false;

        const std::uint64_t seed_maskmov_split_store = seeded(seed_index, 0xE05E);
        if (!tick(run_maskmovdqu_split_exception_case("maskmovdqu_split_pf:" + std::to_string(seed_maskmov_split_store), seed_maskmov_split_store, failure), failure)) return false;

        const std::uint64_t seed_x87_env_split_store = seeded(seed_index, 0xE05F);
        if (!tick(run_x87_env_store_split_exception_case("x87_env_store_split_pf:" + std::to_string(seed_x87_env_split_store), seed_x87_env_split_store, failure), failure)) return false;

        const std::uint64_t seed_x87_env_split_load = seeded(seed_index, 0xE060);
        if (!tick(run_x87_env_load_split_exception_case("x87_env_load_split_pf:" + std::to_string(seed_x87_env_split_load), seed_x87_env_split_load, failure), failure)) return false;

        const std::uint64_t seed_fxsave_split = seeded(seed_index, 0xE061);
        if (!tick(run_fxsave_split_exception_case("fxsave_split_pf:" + std::to_string(seed_fxsave_split), seed_fxsave_split, failure), failure)) return false;

        const std::uint64_t seed_fxrstor_split = seeded(seed_index, 0xE062);
        if (!tick(run_fxrstor_split_exception_case("fxrstor_split_pf:" + std::to_string(seed_fxrstor_split), seed_fxrstor_split, failure), failure)) return false;

        const std::uint64_t seed_far_call_stack_split = seeded(seed_index, 0xE063);
        if (!tick(run_far_call_stack_split_exception_case("far_call_stack_split_pf:" + std::to_string(seed_far_call_stack_split), seed_far_call_stack_split, failure), failure)) return false;

        const std::uint64_t seed_far_ret_stack_split = seeded(seed_index, 0xE064);
        if (!tick(run_multi_pop_stack_split_exception_case("far_ret_stack_split_pf:" + std::to_string(seed_far_ret_stack_split), { 0xCB }, seed_far_ret_stack_split, failure), failure)) return false;

        const std::uint64_t seed_iret_stack_split = seeded(seed_index, 0xE065);
        if (!tick(run_multi_pop_stack_split_exception_case("iret_stack_split_pf:" + std::to_string(seed_iret_stack_split), { 0xCF }, seed_iret_stack_split, failure), failure)) return false;

        const std::uint64_t seed_enter_stack_write = seeded(seed_index, 0xE066);
        if (!tick(run_enter_stack_write_exception_case("enter_stack_write_pf:" + std::to_string(seed_enter_stack_write), seed_enter_stack_write, failure), failure)) return false;

        const std::uint64_t seed_enter_nested_frame = seeded(seed_index, 0xE067);
        if (!tick(run_enter_nested_frame_exception_case("enter_nested_frame_pf:" + std::to_string(seed_enter_nested_frame), seed_enter_nested_frame, failure), failure)) return false;

        const std::uint64_t seed_leave_frame_source = seeded(seed_index, 0xE068);
        if (!tick(run_leave_frame_source_exception_case("leave_frame_source_pf:" + std::to_string(seed_leave_frame_source), seed_leave_frame_source, failure), failure)) return false;

        const std::uint64_t seed_pop_memory_dest = seeded(seed_index, 0xE069);
        if (!tick(run_pop_memory_dest_exception_case("pop_memory_dest_pf:" + std::to_string(seed_pop_memory_dest), seed_pop_memory_dest, failure), failure)) return false;

        const std::uint64_t seed_pop_sreg_invalid = seeded(seed_index, 0xE06A);
        if (!tick(run_pop_sreg_invalid_exception_case("pop_sreg_invalid_gp:" + std::to_string(seed_pop_sreg_invalid), seed_pop_sreg_invalid, failure), failure)) return false;

        const std::uint64_t seed_fninit_nm = seeded(seed_index, 0xE020);
        if (!tick(run_manual_x87_public_cr0_exception_case("public_fninit_nm:" + std::to_string(seed_fninit_nm), fninit, seed_fninit_nm, 0x80000019ull, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;
        if (!tick(run_manual_x87_internal_exception_case("fninit_nm:" + std::to_string(seed_fninit_nm), fninit, seed_fninit_nm, 0x80000019ull, false, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;

        const std::uint64_t seed_fnstsw_nm = seeded(seed_index, 0xE021);
        if (!tick(run_manual_x87_public_cr0_exception_case("public_fnstsw_ax_nm:" + std::to_string(seed_fnstsw_nm), fnstsw_ax_only, seed_fnstsw_nm, 0x80000015ull, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;
        if (!tick(run_manual_x87_internal_exception_case("fnstsw_ax_nm:" + std::to_string(seed_fnstsw_nm), fnstsw_ax_only, seed_fnstsw_nm, 0x80000015ull, false, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;

        const std::uint64_t seed_finit_mf = seeded(seed_index, 0xE022);
        if (!tick(run_manual_x87_internal_exception_case("finit_mf:" + std::to_string(seed_finit_mf), finit_only, seed_finit_mf, 0x80000011ull, true, CPUEAXH_EXCEPTION_MF, failure), failure)) return false;

        const std::uint64_t seed_fstsw_mf = seeded(seed_index, 0xE023);
        if (!tick(run_manual_x87_internal_exception_case("fstsw_ax_mf:" + std::to_string(seed_fstsw_mf), fstsw_ax_only, seed_fstsw_mf, 0x80000011ull, true, CPUEAXH_EXCEPTION_MF, failure), failure)) return false;

        const std::uint64_t seed_fldcw_nm = seeded(seed_index, 0xE024);
        if (!tick(run_manual_x87_public_cr0_exception_case("public_fldcw_nm:" + std::to_string(seed_fldcw_nm), fldcw_only, seed_fldcw_nm, 0x80000019ull, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;
        if (!tick(run_manual_x87_internal_exception_case("fldcw_nm:" + std::to_string(seed_fldcw_nm), fldcw_only, seed_fldcw_nm, 0x80000019ull, false, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;

        const std::uint64_t seed_fldcw_mf = seeded(seed_index, 0xE025);
        if (!tick(run_manual_x87_internal_exception_case("fldcw_mf:" + std::to_string(seed_fldcw_mf), fldcw_only, seed_fldcw_mf, 0x80000011ull, true, CPUEAXH_EXCEPTION_MF, failure), failure)) return false;

        const std::uint64_t seed_fstenv_mf = seeded(seed_index, 0xE028);
        if (!tick(run_manual_x87_env_store_case(
            "fstenv_mf:" + std::to_string(seed_fstenv_mf),
            fstenv_rsp,
            seed_fstenv_mf,
            0x037Fu,
            0x0001u,
            0xFFFFu,
            0,
            0,
            0,
            true,
            true,
            failure), failure)) return false;

        const std::uint64_t seed_fldenv_nm = seeded(seed_index, 0xE02B);
        if (!tick(run_manual_x87_public_cr0_exception_case("public_fldenv_nm:" + std::to_string(seed_fldenv_nm), fldenv_only, seed_fldenv_nm, 0x80000019ull, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;
        if (!tick(run_manual_x87_internal_exception_case("fldenv_nm:" + std::to_string(seed_fldenv_nm), fldenv_only, seed_fldenv_nm, 0x80000019ull, false, CPUEAXH_EXCEPTION_NM, failure), failure)) return false;

        const std::uint64_t seed_fldenv_mf = seeded(seed_index, 0xE02C);
        if (!tick(run_manual_x87_internal_exception_case("fldenv_mf:" + std::to_string(seed_fldenv_mf), fldenv_only, seed_fldenv_mf, 0x80000011ull, true, CPUEAXH_EXCEPTION_MF, failure), failure)) return false;

        const std::uint64_t seed_insb = seeded(seed_index, 0xE02D);
        const std::uint64_t insb_address = kGuestStackBase + 0x80;
        const std::uint8_t insb0 = static_cast<std::uint8_t>(seeded(seed_insb, 0xD1) & 0xFFu);
        const std::uint8_t insb1 = static_cast<std::uint8_t>(seeded(seed_insb, 0xD2) & 0xFFu);
        const std::uint8_t insb2 = static_cast<std::uint8_t>(seeded(seed_insb, 0xD3) & 0xFFu);
        if (!tick(run_public_ins_escape_case(
            "insb_rep_edi:" + std::to_string(seed_insb),
            rep_insb_edi,
            seed_insb,
            static_cast<std::uint16_t>(seeded(seed_insb, 0xD0) & 0xFFFFu),
            0xABCDEF0000000000ull | static_cast<std::uint32_t>(insb_address),
            0x1234567800000003ull,
            0,
            std::vector<std::uint32_t>{ insb0, insb1, insb2 },
            insb_address,
            std::vector<std::uint8_t>{ insb0, insb1, insb2 },
            static_cast<std::uint32_t>(insb_address + 3),
            0,
            failure), failure)) return false;

        const std::uint64_t seed_insw = seeded(seed_index, 0xE02E);
        const std::uint64_t insw_address = kGuestStackBase + 0xA2;
        const std::uint16_t insw_value = static_cast<std::uint16_t>(seeded(seed_insw, 0xD5) & 0xFFFFu);
        if (!tick(run_public_ins_escape_case(
            "insw_df_edi:" + std::to_string(seed_insw),
            insw_df_edi,
            seed_insw,
            static_cast<std::uint16_t>(seeded(seed_insw, 0xD4) & 0xFFFFu),
            0xCAFEBABE00000000ull | static_cast<std::uint32_t>(insw_address),
            seeded(seed_insw, 0xD6),
            RFLAGS_DF,
            std::vector<std::uint32_t>{ insw_value },
            insw_address,
            std::vector<std::uint8_t>{
                static_cast<std::uint8_t>(insw_value & 0xFFu),
                static_cast<std::uint8_t>((insw_value >> 8) & 0xFFu)
            },
            static_cast<std::uint32_t>(insw_address - 2),
            seeded(seed_insw, 0xD6),
            failure), failure)) return false;

        const std::uint64_t seed_insd = seeded(seed_index, 0xE02F);
        const std::uint64_t insd_address = kGuestStackBase + 0xC0;
        const std::uint32_t insd_value = static_cast<std::uint32_t>(seeded(seed_insd, 0xD8) & 0xFFFFFFFFu);
        const std::uint64_t insd_rcx = seeded(seed_insd, 0xD9);
        if (!tick(run_public_ins_escape_case(
            "insd_rdi:" + std::to_string(seed_insd),
            insd_rdi,
            seed_insd,
            static_cast<std::uint16_t>(seeded(seed_insd, 0xD7) & 0xFFFFu),
            insd_address,
            insd_rcx,
            0,
            std::vector<std::uint32_t>{ insd_value },
            insd_address,
            std::vector<std::uint8_t>{
                static_cast<std::uint8_t>(insd_value & 0xFFu),
                static_cast<std::uint8_t>((insd_value >> 8) & 0xFFu),
                static_cast<std::uint8_t>((insd_value >> 16) & 0xFFu),
                static_cast<std::uint8_t>((insd_value >> 24) & 0xFFu)
            },
            insd_address + 4,
            insd_rcx,
            failure), failure)) return false;

        const std::uint64_t seed_popcnt_lock = seeded(seed_index, 0xE033);
        if (!tick(run_manual_exception_case_public("public_popcnt_lock_ud:" + std::to_string(seed_popcnt_lock), invalid_popcnt_lock, seed_popcnt_lock, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("popcnt_lock_ud:" + std::to_string(seed_popcnt_lock), invalid_popcnt_lock, seed_popcnt_lock, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_palignr_lock = seeded(seed_index, 0xE03B);
        if (!tick(run_manual_exception_case_public("public_palignr_lock_ud:" + std::to_string(seed_palignr_lock), invalid_palignr_lock, seed_palignr_lock, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("palignr_lock_ud:" + std::to_string(seed_palignr_lock), invalid_palignr_lock, seed_palignr_lock, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_palignr_misaligned = seeded(seed_index, 0xE03C);
        if (!tick(run_manual_exception_case_public("public_palignr_misaligned_gp:" + std::to_string(seed_palignr_misaligned), invalid_palignr_misaligned, seed_palignr_misaligned, CPUEAXH_EXCEPTION_GP, failure), failure)) return false;
        if (!tick(run_manual_exception_case("palignr_misaligned_gp:" + std::to_string(seed_palignr_misaligned), invalid_palignr_misaligned, seed_palignr_misaligned, CPUEAXH_EXCEPTION_GP, failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrw_ymm = seeded(seed_index, 0xE040);
        if (!tick(run_manual_exception_case_public("public_evex_vpinsrw_ymm_ud:" + std::to_string(seed_evex_vpinsrw_ymm), invalid_evex_vpinsrw_ymm, seed_evex_vpinsrw_ymm, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("evex_vpinsrw_ymm_ud:" + std::to_string(seed_evex_vpinsrw_ymm), invalid_evex_vpinsrw_ymm, seed_evex_vpinsrw_ymm, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrw_k1 = seeded(seed_index, 0xE041);
        if (!tick(run_manual_exception_case_public("public_evex_vpinsrw_k1_ud:" + std::to_string(seed_evex_vpinsrw_k1), invalid_evex_vpinsrw_k1, seed_evex_vpinsrw_k1, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("evex_vpinsrw_k1_ud:" + std::to_string(seed_evex_vpinsrw_k1), invalid_evex_vpinsrw_k1, seed_evex_vpinsrw_k1, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_vpinsrb_l = seeded(seed_index, 0xE056);
        if (!tick(run_manual_exception_case_public("public_vpinsrb_l_ud:" + std::to_string(seed_vpinsrb_l), invalid_vpinsrb_l, seed_vpinsrb_l, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("vpinsrb_l_ud:" + std::to_string(seed_vpinsrb_l), invalid_vpinsrb_l, seed_vpinsrb_l, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrb_ymm = seeded(seed_index, 0xE057);
        if (!tick(run_manual_exception_case_public("public_evex_vpinsrb_ymm_ud:" + std::to_string(seed_evex_vpinsrb_ymm), invalid_evex_vpinsrb_ymm, seed_evex_vpinsrb_ymm, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("evex_vpinsrb_ymm_ud:" + std::to_string(seed_evex_vpinsrb_ymm), invalid_evex_vpinsrb_ymm, seed_evex_vpinsrb_ymm, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_evex_vpinsrd_k1 = seeded(seed_index, 0xE058);
        if (!tick(run_manual_exception_case_public("public_evex_vpinsrd_k1_ud:" + std::to_string(seed_evex_vpinsrd_k1), invalid_evex_vpinsrd_k1, seed_evex_vpinsrd_k1, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("evex_vpinsrd_k1_ud:" + std::to_string(seed_evex_vpinsrd_k1), invalid_evex_vpinsrd_k1, seed_evex_vpinsrd_k1, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        if (features.aes && features.avx) {
            const std::uint64_t seed_vaesimc_vvvv = seeded(seed_index, 0xE05D);
            if (!tick(run_manual_exception_case_public("public_vaesimc_vvvv_ud:" + std::to_string(seed_vaesimc_vvvv), invalid_vaesimc_vvvv, seed_vaesimc_vvvv, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
            if (!tick(run_manual_exception_case("vaesimc_vvvv_ud:" + std::to_string(seed_vaesimc_vvvv), invalid_vaesimc_vvvv, seed_vaesimc_vvvv, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        }

        const std::uint64_t seed_vpextr_l = seeded(seed_index, 0xE04B);
        if (!tick(run_manual_exception_case_public("public_vpextrb_l_ud:" + std::to_string(seed_vpextr_l), invalid_vpextrb_l, seed_vpextr_l, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("vpextrb_l_ud:" + std::to_string(seed_vpextr_l), invalid_vpextrb_l, seed_vpextr_l, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;

        const std::uint64_t seed_evex_vpextr_vvvv = seeded(seed_index, 0xE04C);
        if (!tick(run_manual_exception_case_public("public_evex_vpextrd_vvvv_ud:" + std::to_string(seed_evex_vpextr_vvvv), invalid_evex_vpextrd_vvvv, seed_evex_vpextr_vvvv, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
        if (!tick(run_manual_exception_case("evex_vpextrd_vvvv_ud:" + std::to_string(seed_evex_vpextr_vvvv), invalid_evex_vpextrd_vvvv, seed_evex_vpextr_vvvv, CPUEAXH_EXCEPTION_UD, failure), failure)) return false;
    }

    for (std::uint64_t seed_index = 0; seed_index < kHostStackSeedCount; ++seed_index) {
        Failure failure;
        const std::uint64_t seed7 = seeded(seed_index, 0xE251);
        if (!tick(run_host_stack_roundtrip_case("host_stack_roundtrip:" + std::to_string(seed7), seed7, failure), failure)) return false;

        const std::uint64_t seed_xchg_cache = seeded(seed_index, 0xE261);
        if (!tick(run_cached_rmw_recompute_case("cached_xchg_mem_recompute:" + std::to_string(seed_xchg_cache), seed_xchg_cache, 0, failure), failure)) return false;

        const std::uint64_t seed_xadd_cache = seeded(seed_index, 0xE262);
        if (!tick(run_cached_rmw_recompute_case("cached_xadd_mem_recompute:" + std::to_string(seed_xadd_cache), seed_xadd_cache, 1, failure), failure)) return false;

        const std::uint64_t seed_cmpxchg_cache = seeded(seed_index, 0xE263);
        if (!tick(run_cached_rmw_recompute_case("cached_cmpxchg_mem_recompute:" + std::to_string(seed_cmpxchg_cache), seed_cmpxchg_cache, 2, failure), failure)) return false;
    }

    for (std::uint64_t seed_index = 0; seed_index < kContextApiSeedCount; ++seed_index) {
        Failure failure;
        const std::uint64_t seed8 = seeded(seed_index, 0xE301);
        if (!tick(run_context_api_case("context_api:" + std::to_string(seed8), seed8, failure), failure)) return false;
    }

    return all_ok;
}

inline bool emit_host_mov_reg_imm64(std::vector<std::uint8_t>& code, std::uint8_t reg_low3, std::uint64_t imm) {
    if (reg_low3 > 7) {
        return false;
    }
    code.push_back(0x48);
    code.push_back(static_cast<std::uint8_t>(0xB8u + reg_low3));
    for (int shift = 0; shift < 64; shift += 8) {
        code.push_back(static_cast<std::uint8_t>((imm >> shift) & 0xFFu));
    }
    return true;
}

inline bool run_host_stack_roundtrip_case(const std::string& name, std::uint64_t seed, Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    std::uint8_t* code_page = nullptr;
    std::uint8_t* stack_page = nullptr;

    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    bool ok = false;
    do {
        code_page = static_cast<std::uint8_t*>(::VirtualAlloc(nullptr, kCodePageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        stack_page = static_cast<std::uint8_t*>(::VirtualAlloc(nullptr, kStackSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!code_page || !stack_page) {
            failure.case_name = name;
            failure.detail = "VirtualAlloc failed";
            break;
        }

        std::memset(code_page, 0xCC, kCodePageSize);
        std::memset(stack_page, 0, kStackSize);

        const std::uint64_t r8 = seeded(seed, 0x7101);
        const std::uint64_t r11 = seeded(seed, 0x7102);
        const std::uint64_t expected_rbx = ~r8;
        const std::vector<std::uint8_t> code = {
            0x4C, 0x89, 0xC3,
            0x48, 0xF7, 0xD3,
            0x41, 0x53,
            0x48, 0x8B, 0x44, 0x24, 0x08,
            0x4C, 0x31, 0xC8,
            0x41, 0x5A,
            0x4D, 0x31, 0xDA,
            0x4C, 0x09, 0xD0,
            0xC3,
        };
        std::memcpy(code_page, code.data(), code.size());

        err = cpueaxh_set_memory_mode(engine, CPUEAXH_MEMORY_MODE_HOST);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "cpueaxh_set_memory_mode(host) failed";
            break;
        }

        std::uint64_t rsp = reinterpret_cast<std::uint64_t>(stack_page) + kStackSize - 0x80 - sizeof(std::uint64_t);
        *reinterpret_cast<std::uint64_t*>(rsp) = 0;
        const std::uint64_t initial_rsp = rsp;
        std::uint64_t rbp = rsp;
        std::uint64_t rip = reinterpret_cast<std::uint64_t>(code_page);
        std::uint64_t r9 = CPUEAXH_EMU_RETURN_MAGIC;
        std::uint64_t rax = 0;
        std::uint64_t rbx = 0;
        std::uint64_t r10 = 0;

        if (cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RSP, &rsp) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RBP, &rbp) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RIP, &rip) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RAX, &rax) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_RBX, &rbx) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_R8, &r8) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_R9, &r9) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_R10, &r10) != CPUEAXH_ERR_OK ||
            cpueaxh_reg_write(engine, CPUEAXH_X86_REG_R11, &r11) != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "host stack register initialization failed";
            break;
        }

        err = cpueaxh_emu_start_function(engine, 0, 0, 1024);
        if (err != CPUEAXH_ERR_OK || cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "host stack execution failed";
            break;
        }

        std::uint64_t observed_rax = 0;
        std::uint64_t observed_rbx = 0;
        std::uint64_t observed_rsp = 0;
        std::uint64_t observed_r10 = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_RAX, observed_rax) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RBX, observed_rbx) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_RSP, observed_rsp) ||
            !read_engine_reg(engine, CPUEAXH_X86_REG_R10, observed_r10)) {
            failure.case_name = name;
            failure.detail = "host stack register read failed";
            break;
        }

        if (observed_rax != 0) {
            failure.case_name = name;
            failure.detail = "host stack return-slot verification failed";
            break;
        }
        if (observed_rbx != expected_rbx) {
            failure.case_name = name;
            failure.detail = "host stack not result mismatch";
            break;
        }
        if (observed_r10 != 0) {
            failure.case_name = name;
            failure.detail = "host stack pop verification failed";
            break;
        }
        if (observed_rsp != initial_rsp + sizeof(std::uint64_t)) {
            failure.case_name = name;
            failure.detail = "host stack pointer did not restore";
            break;
        }
        if (*reinterpret_cast<std::uint64_t*>(initial_rsp) != 0) {
            failure.case_name = name;
            failure.detail = "host stack return slot mutated";
            break;
        }

        ok = true;
    } while (false);

    if (stack_page) {
        ::VirtualFree(stack_page, 0, MEM_RELEASE);
    }
    if (code_page) {
        ::VirtualFree(code_page, 0, MEM_RELEASE);
    }
    if (engine) {
        cpueaxh_close(engine);
    }
    return ok;
}

inline void append_u32(std::vector<std::uint8_t>& code, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        code.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

inline void append_u64(std::vector<std::uint8_t>& code, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        code.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

inline bool run_cached_rmw_recompute_case(const std::string& name, std::uint64_t seed, std::uint8_t opcode_kind, Failure& failure) {
    cpueaxh_engine* engine = nullptr;
    cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine);
    if (err != CPUEAXH_ERR_OK) {
        failure.case_name = name;
        failure.detail = "cpueaxh_open failed";
        return false;
    }

    constexpr std::size_t data_offset = 0x80;
    const std::uint64_t data_address = kGuestCodeBase + data_offset;
    std::vector<std::uint8_t> code;
    code.reserve(data_offset + 8);

    code.push_back(0x48); code.push_back(0xBB); append_u64(code, data_address); // mov rbx, data
    code.push_back(0xB9); append_u32(code, 2);                                  // mov ecx, 2

    if (opcode_kind == 2) {
        code.push_back(0xB8); append_u32(code, 5);                              // mov eax, 5
        code.push_back(0xBA); append_u32(code, 9);                              // mov edx, 9
    }
    else {
        code.push_back(0xB8); append_u32(code, opcode_kind == 0 ? 0x10u : 1u);   // mov eax, imm32
    }

    const std::size_t loop_offset = code.size();
    if (opcode_kind == 0) {
        code.push_back(0x87); code.push_back(0x03);                             // xchg dword ptr [rbx], eax
    }
    else if (opcode_kind == 1) {
        code.push_back(0x0F); code.push_back(0xC1); code.push_back(0x03);        // xadd dword ptr [rbx], eax
    }
    else {
        code.push_back(0x0F); code.push_back(0xB1); code.push_back(0x13);        // cmpxchg dword ptr [rbx], edx
    }
    code.push_back(0x48); code.push_back(0x83); code.push_back(0xC3); code.push_back(0x04); // add rbx, 4
    code.push_back(0xFF); code.push_back(0xC9);                                  // dec ecx
    code.push_back(0x75);
    const std::ptrdiff_t jnz_next = static_cast<std::ptrdiff_t>(code.size() + 1);
    code.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(static_cast<std::ptrdiff_t>(loop_offset) - jnz_next))); // jnz loop
    code.push_back(0xC3);

    while (code.size() < data_offset) {
        code.push_back(0xCC);
    }

    const std::uint32_t initial0 = opcode_kind == 2 ? 5u : (opcode_kind == 1 ? 10u : 0x11111111u);
    const std::uint32_t initial1 = opcode_kind == 2 ? 7u : (opcode_kind == 1 ? 20u : 0x22222222u);
    append_u32(code, initial0);
    append_u32(code, initial1);

    bool ok = false;
    do {
        cpueaxh_x86_context initial = make_initial_context(seed);
        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        if (!initialize_manual_engine(engine, code, initial, guest_rsp, failure, name)) {
            break;
        }

        err = cpueaxh_emu_start_function(engine, kGuestCodeBase, 0, 1000);
        if (err != CPUEAXH_ERR_OK || cpueaxh_code_exception(engine) != CPUEAXH_EXCEPTION_NONE) {
            failure.case_name = name;
            failure.detail = "cached rmw execution failed";
            break;
        }

        std::uint32_t memory0 = 0;
        std::uint32_t memory1 = 0;
        if (cpueaxh_mem_read(engine, data_address, &memory0, sizeof(memory0)) != CPUEAXH_ERR_OK ||
            cpueaxh_mem_read(engine, data_address + 4, &memory1, sizeof(memory1)) != CPUEAXH_ERR_OK) {
            failure.case_name = name;
            failure.detail = "cached rmw memory readback failed";
            break;
        }

        std::uint64_t rax = 0;
        if (!read_engine_reg(engine, CPUEAXH_X86_REG_RAX, rax)) {
            failure.case_name = name;
            failure.detail = "cached rmw rax readback failed";
            break;
        }

        std::uint32_t expected0 = 0;
        std::uint32_t expected1 = 0;
        std::uint32_t expected_eax = 0;
        if (opcode_kind == 0) {
            expected0 = 0x10u;
            expected1 = 0x11111111u;
            expected_eax = 0x22222222u;
        }
        else if (opcode_kind == 1) {
            expected0 = 11u;
            expected1 = 30u;
            expected_eax = 20u;
        }
        else {
            expected0 = 9u;
            expected1 = 7u;
            expected_eax = 7u;
        }

        if (memory0 != expected0 || memory1 != expected1 || static_cast<std::uint32_t>(rax) != expected_eax) {
            failure.case_name = name;
            failure.detail = "cached rmw replay used stale effective address";
            break;
        }

        ok = true;
    } while (false);

    cpueaxh_close(engine);
    return ok;
}

class Harness {
public:
    Harness() {
        native_code_ = static_cast<std::uint8_t*>(::VirtualAlloc(nullptr, kCodePageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        native_stack_ = static_cast<std::uint8_t*>(::VirtualAlloc(nullptr, kStackSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        cpueaxh_err err = cpueaxh_open(CPUEAXH_ARCH_X86, CPUEAXH_MODE_64, &engine_);
        if (err != CPUEAXH_ERR_OK) {
            init_error_ = std::string("cpueaxh_open failed: ") + std::to_string(err);
            return;
        }
        err = cpueaxh_set_memory_mode(engine_, CPUEAXH_MEMORY_MODE_GUEST);
        if (err != CPUEAXH_ERR_OK) {
            init_error_ = std::string("cpueaxh_set_memory_mode failed: ") + std::to_string(err);
            return;
        }
        err = cpueaxh_mem_map(engine_, kGuestCodeBase, kCodePageSize, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE | CPUEAXH_PROT_EXEC);
        if (err != CPUEAXH_ERR_OK) {
            init_error_ = std::string("cpueaxh_mem_map code failed: ") + std::to_string(err);
            return;
        }
        err = cpueaxh_mem_map(engine_, kGuestStackBase, kStackSize, CPUEAXH_PROT_READ | CPUEAXH_PROT_WRITE);
        if (err != CPUEAXH_ERR_OK) {
            init_error_ = std::string("cpueaxh_mem_map stack failed: ") + std::to_string(err);
            return;
        }
        ready_ = native_code_ != nullptr && native_stack_ != nullptr;
        if (!ready_ && init_error_.empty()) {
            init_error_ = "memory allocation failed";
        }
    }

    ~Harness() {
        if (engine_) {
            cpueaxh_close(engine_);
        }
        if (native_code_) {
            ::VirtualFree(native_code_, 0, MEM_RELEASE);
        }
        if (native_stack_) {
            ::VirtualFree(native_stack_, 0, MEM_RELEASE);
        }
    }

    bool ready() const {
        return ready_;
    }

    const std::string& init_error() const {
        return init_error_;
    }

    bool run_case(const BuiltCase& built, Failure& failure) {
        if (built.image.empty() || built.image.size() > kCodePageSize) {
            failure.case_name = built.spec.name;
            failure.detail = "code generation failed";
            return false;
        }

        std::memset(native_code_, 0, kCodePageSize);
        std::memset(native_stack_, 0, kStackSize);
        std::memcpy(native_code_, built.image.data(), built.image.size());

        const std::uint64_t guest_rsp = kGuestStackBase + kInitialRspOffset;
        const std::uint64_t native_rsp = reinterpret_cast<std::uint64_t>(native_stack_) + kInitialRspOffset;

        cpueaxh_x86_context native_context = built.initial_context;
        native_context.regs[static_cast<std::size_t>(Reg::RSP)] = native_rsp;
        native_context.rip = reinterpret_cast<std::uint64_t>(native_code_);
        native_context.rflags = built.initial_context.rflags;
        native_context.mxcsr = static_cast<std::uint32_t>(kInitialMxcsr);

        TestBridgeBlock block{};
        if (cpueaxh_test_run_native(&native_context, native_code_, &block) != 0) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = "native execution failed";
            return false;
        }
        static const std::vector<std::uint8_t> zero_code(kCodePageSize, 0);
        static const std::vector<std::uint8_t> zero_stack(kStackSize, 0);
        cpueaxh_err err = cpueaxh_mem_write(engine_, kGuestCodeBase, zero_code.data(), zero_code.size());
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = built.spec.name;
            failure.detail = "guest zero code failed";
            return false;
        }
        err = cpueaxh_mem_write(engine_, kGuestStackBase, zero_stack.data(), zero_stack.size());
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = built.spec.name;
            failure.detail = "guest zero stack failed";
            return false;
        }
        err = cpueaxh_mem_write(engine_, kGuestCodeBase, built.image.data(), built.image.size());
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = built.spec.name;
            failure.detail = "guest write code failed";
            return false;
        }

        if (!write_reg(CPUEAXH_X86_REG_RAX, built.initial_context.regs[0])) return fail_reg_write(failure, built, "rax");
        if (!write_reg(CPUEAXH_X86_REG_RCX, built.initial_context.regs[1])) return fail_reg_write(failure, built, "rcx");
        if (!write_reg(CPUEAXH_X86_REG_RDX, built.initial_context.regs[2])) return fail_reg_write(failure, built, "rdx");
        if (!write_reg(CPUEAXH_X86_REG_RBX, built.initial_context.regs[3])) return fail_reg_write(failure, built, "rbx");
        if (!write_reg(CPUEAXH_X86_REG_RSP, guest_rsp)) return fail_reg_write(failure, built, "rsp");
        if (!write_reg(CPUEAXH_X86_REG_RBP, built.initial_context.regs[5])) return fail_reg_write(failure, built, "rbp");
        if (!write_reg(CPUEAXH_X86_REG_RSI, built.initial_context.regs[6])) return fail_reg_write(failure, built, "rsi");
        if (!write_reg(CPUEAXH_X86_REG_RDI, built.initial_context.regs[7])) return fail_reg_write(failure, built, "rdi");
        if (!write_reg(CPUEAXH_X86_REG_R8, built.initial_context.regs[8])) return fail_reg_write(failure, built, "r8");
        if (!write_reg(CPUEAXH_X86_REG_R9, built.initial_context.regs[9])) return fail_reg_write(failure, built, "r9");
        if (!write_reg(CPUEAXH_X86_REG_R10, built.initial_context.regs[10])) return fail_reg_write(failure, built, "r10");
        if (!write_reg(CPUEAXH_X86_REG_R11, built.initial_context.regs[11])) return fail_reg_write(failure, built, "r11");
        if (!write_reg(CPUEAXH_X86_REG_R12, built.initial_context.regs[12])) return fail_reg_write(failure, built, "r12");
        if (!write_reg(CPUEAXH_X86_REG_R13, built.initial_context.regs[13])) return fail_reg_write(failure, built, "r13");
        if (!write_reg(CPUEAXH_X86_REG_R14, built.initial_context.regs[14])) return fail_reg_write(failure, built, "r14");
        if (!write_reg(CPUEAXH_X86_REG_R15, built.initial_context.regs[15])) return fail_reg_write(failure, built, "r15");
        if (!write_reg(CPUEAXH_X86_REG_RIP, kGuestCodeBase)) return fail_reg_write(failure, built, "rip");
        if (!write_reg(CPUEAXH_X86_REG_EFLAGS, built.initial_context.rflags)) return fail_reg_write(failure, built, "rflags");

        err = cpueaxh_emu_start_function(engine_, kGuestCodeBase, 0, 100000);
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = std::string("guest execution failed: ") + std::to_string(err) + " exc=" + std::to_string(cpueaxh_code_exception(engine_));
            return false;
        }

        std::array<std::uint64_t, 16> guest_regs{};
        for (std::size_t index = 0; index < guest_regs.size(); ++index) {
            if (!read_reg(static_cast<int>(index), guest_regs[index])) {
                failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
                failure.detail = "guest read register failed";
                return false;
            }
        }
        std::uint64_t guest_flags = 0;
        if (!read_reg(CPUEAXH_X86_REG_EFLAGS, guest_flags)) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = "guest read flags failed";
            return false;
        }
        std::uint64_t guest_rip = 0;
        if (!read_reg(CPUEAXH_X86_REG_RIP, guest_rip)) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = "guest read rip failed";
            return false;
        }
        std::uint32_t guest_mxcsr = 0;
        if (!read_reg32(CPUEAXH_X86_REG_MXCSR, guest_mxcsr)) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = "guest read mxcsr failed";
            return false;
        }

        std::vector<std::uint8_t> guest_data(kDataSize);
        err = cpueaxh_mem_read(engine_, kGuestCodeBase + built.data_offset, guest_data.data(), guest_data.size());
        if (err != CPUEAXH_ERR_OK) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = "guest read data failed";
            return false;
        }

        const std::uint8_t* native_data = native_code_ + built.data_offset;
        const std::vector<std::uint8_t> native_data_snapshot(native_data, native_data + kDataSize);
        std::array<std::uint64_t, 16> normalized_native_regs{};
        for (std::size_t index = 0; index < normalized_native_regs.size(); ++index) {
            normalized_native_regs[index] = normalize_native_value(native_context.regs[index], built);
        }
        const std::uint64_t normalized_native_rip = normalize_native_value(native_context.rip, built);
        attach_generated_result_state(
            failure,
            normalized_native_regs,
            normalized_native_rip,
            native_context.rflags,
            native_context.mxcsr,
            native_data_snapshot,
            guest_regs,
            guest_rip,
            guest_flags,
            guest_mxcsr,
            guest_data);
        for (std::size_t index = 0; index < 16; ++index) {
            const std::uint64_t guest_value = guest_regs[index];
            const std::uint64_t native_value = normalized_native_regs[index];
            if (guest_value != native_value) {
                failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
                failure.detail = std::string(reg_name(static_cast<Reg>(index))) + " guest=" + hex64(guest_value) + " native=" + hex64(native_value);
                return false;
            }
        }

        const std::uint64_t guest_masked = guest_flags & built.spec.flag_mask;
        const std::uint64_t native_masked = native_context.rflags & built.spec.flag_mask;
        if (guest_masked != native_masked) {
            failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
            failure.detail = std::string("flags guest=") + hex64(guest_masked) + " native=" + hex64(native_masked);
            return false;
        }

        for (std::size_t index = 0; index < guest_data.size(); ++index) {
            if (guest_data[index] != native_data[index]) {
                failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
                failure.detail = std::string("data[") + std::to_string(index) + "] guest=" + hex8(guest_data[index]) + " native=" + hex8(native_data[index]);
                return false;
            }
        }

        return true;
    }

private:
    std::uint64_t normalize_native_value(std::uint64_t value, const BuiltCase& built) const {
        const std::uint64_t native_code_begin = reinterpret_cast<std::uint64_t>(native_code_);
        const std::uint64_t native_code_end = native_code_begin + kCodePageSize;
        if (value >= native_code_begin && value < native_code_end) {
            return kGuestCodeBase + (value - native_code_begin);
        }

        const std::uint64_t native_stack_begin = reinterpret_cast<std::uint64_t>(native_stack_);
        const std::uint64_t native_stack_end = native_stack_begin + kStackSize;
        if (value >= native_stack_begin && value < native_stack_end) {
            return kGuestStackBase + (value - native_stack_begin);
        }

        (void)built;
        return value;
    }

    bool write_reg(int reg, std::uint64_t value) {
        return cpueaxh_reg_write(engine_, reg, &value) == CPUEAXH_ERR_OK;
    }

    bool read_reg(int reg, std::uint64_t& value) {
        return cpueaxh_reg_read(engine_, reg, &value) == CPUEAXH_ERR_OK;
    }

    bool read_reg32(int reg, std::uint32_t& value) {
        return cpueaxh_reg_read(engine_, reg, &value) == CPUEAXH_ERR_OK;
    }

    bool fail_reg_write(Failure& failure, const BuiltCase& built, const char* name) {
        failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
        failure.detail = std::string("guest write ") + name + " failed";
        return false;
    }

    cpueaxh_engine* engine_ = nullptr;
    std::uint8_t* native_code_ = nullptr;
    std::uint8_t* native_stack_ = nullptr;
    bool ready_ = false;
    std::string init_error_;
};

inline const ProgramSpec* find_spec_by_name(const std::vector<ProgramSpec>& specs, const std::string& name) {
    for (const ProgramSpec& spec : specs) {
        if (spec.name == name) {
            return &spec;
        }
    }
    return nullptr;
}

} // namespace cpueaxh_test

