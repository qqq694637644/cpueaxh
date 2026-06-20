#pragma once
// Split from test/demo/framework.hpp: generated spec catalog, case builder, and generated compare harness.
// This header is self-contained and may be included directly; framework.hpp is only an umbrella include.

#include "code_builder.hpp"

namespace cpueaxh_test {

inline std::vector<ProgramSpec> make_specs(const HostFeatures& features) {
    std::vector<ProgramSpec> specs;
    const BinaryOp binary_ops[] = {
        BinaryOp::Add, BinaryOp::Adc, BinaryOp::Sub, BinaryOp::Sbb,
        BinaryOp::And, BinaryOp::Or, BinaryOp::Xor, BinaryOp::Cmp, BinaryOp::Test
    };
    const BinaryOp imm_ops[] = {
        BinaryOp::Add, BinaryOp::Sub, BinaryOp::And, BinaryOp::Or,
        BinaryOp::Xor, BinaryOp::Cmp, BinaryOp::Test
    };
    const UnaryOp unary_ops[] = { UnaryOp::Inc, UnaryOp::Dec, UnaryOp::Neg, UnaryOp::Not, UnaryOp::Mul, UnaryOp::Imul, UnaryOp::Div, UnaryOp::Idiv };
    const ShiftOp shift_ops[] = { ShiftOp::Rol, ShiftOp::Ror, ShiftOp::Rcl, ShiftOp::Rcr, ShiftOp::Shl, ShiftOp::Shr, ShiftOp::Sar };
    for (const BinaryOp op : binary_ops) {
        for (std::size_t index = 0; index < kBinaryPairs.size(); ++index) {
            specs.push_back({ Family::BinaryRegReg, static_cast<std::uint32_t>(op), static_cast<std::uint32_t>(index), binary_flag_mask(op), std::string(binary_name(op)) + "_rr_" + kBinaryPairs[index].name });
        }
    }
    for (const BinaryOp op : imm_ops) {
        for (std::size_t index = 0; index < kCoreRegs.size(); ++index) {
            specs.push_back({ Family::BinaryRegImm, static_cast<std::uint32_t>(op), static_cast<std::uint32_t>(index), binary_flag_mask(op), std::string(binary_name(op)) + "_ri_" + kCoreRegs[index].name });
        }
    }
    for (const UnaryOp op : unary_ops) {
        for (std::size_t index = 0; index < kUnaryRegs.size(); ++index) {
            specs.push_back({ Family::UnaryReg, static_cast<std::uint32_t>(op), static_cast<std::uint32_t>(index), unary_flag_mask(op), std::string(unary_name(op)) + "_" + kUnaryRegs[index].name });
        }
    }
    for (const ShiftOp op : shift_ops) {
        for (std::size_t index = 0; index < kCoreRegs.size(); ++index) {
            const std::uint64_t mask = shift_flag_mask(op);
            specs.push_back({ Family::ShiftImm, static_cast<std::uint32_t>(op), static_cast<std::uint32_t>(index), mask, std::string(shift_name(op)) + "_imm_" + kCoreRegs[index].name });
            specs.push_back({ Family::ShiftCl, static_cast<std::uint32_t>(op), static_cast<std::uint32_t>(index), mask, std::string(shift_name(op)) + "_cl_" + kCoreRegs[index].name });
        }
    }
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtImm), 0, kBitTestMask, "bt_imm_rax" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtReg), 0, kBitTestMask, "bt_reg_r8_rcx" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtMemImm), 0, kBitTestMask, "bt_imm_m64" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtsImm), 0, kBitTestMask, "bts_imm_r10" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtsReg), 0, kBitTestMask, "bts_reg_r11_rcx" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtsMemImm), 0, kBitTestMask, "bts_imm_m64" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtrImm), 0, kBitTestMask, "btr_imm_r12" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtrReg), 0, kBitTestMask, "btr_reg_r13_rcx" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtrMemImm), 0, kBitTestMask, "btr_imm_m64" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtcImm), 0, kBitTestMask, "btc_imm_r14" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtcReg), 0, kBitTestMask, "btc_reg_r15_rcx" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BtcMemImm), 0, kBitTestMask, "btc_imm_m64" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::Bsf), 0, kBitScanMask, "bsf_rdx_rbx" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::Bsr), 0, kBitScanMask, "bsr_r9_r10" });
    if (features.popcnt) {
        specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::Popcnt), 0, kStatusMask, "popcnt_rdx_rbx" });
    }
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::Bswap), 0, kStatusMask, "bswap_r11" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BsfAlt), 0, kBitScanMask, "bsf_r9_r10" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::BswapAlt), 0, kStatusMask, "bswap_rax" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::Tzcnt), 0, features.bmi1 ? kBitCountMask : kBitScanMask, features.bmi1 ? "tzcnt_rdx_rbx" : "f3_bsf_rdx_rbx" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::Lzcnt), 0, features.lzcnt ? kBitCountMask : kBitScanMask, features.lzcnt ? "lzcnt_r9_r10" : "f3_bsr_r9_r10" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::ImulRegReg), 0, kRotationMask, "imul_r10_r11" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::ImulRegImm8), 0, kRotationMask, "imul_r12_r13_imm8" });
    specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(BitOp::ImulRegImm32), 0, kRotationMask, "imul_r14_r15_imm32" });
    if (features.sse42) {
        for (const Crc32CaseSpec& crc32_case : kCrc32Cases) {
            specs.push_back({ Family::BitOps, static_cast<std::uint32_t>(crc32_case.op), 0, 0, crc32_case.name });
        }
    }
    for (const CondSpec& condition : kConditions) {
        for (int expected = 0; expected < 2; ++expected) {
            const bool should_be_true = expected != 0;
            const std::uint32_t variant = condition_variant(condition.code, should_be_true);
            specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Setcc), variant, kStatusMask, condition_case_name("set", condition, should_be_true) });
            specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Cmovcc), variant, kStatusMask, condition_case_name("cmov", condition, should_be_true) });
            specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Jcc), variant, kStatusMask, condition_case_name("j", condition, should_be_true) });
        }
    }
    specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Cmc), 0, kFlagCF, "cmc" });
    specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Clc), 0, kFlagCF, "clc" });
    specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Stc), 0, kFlagCF, "stc" });
    specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Cld), 0, kFlagDF, "cld" });
    specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Lahf), 0, 0, "lahf" });
    specs.push_back({ Family::FlagOps, static_cast<std::uint32_t>(FlagProgram::Sahf), 0, kFlagCF | kFlagPF | kFlagAF | kFlagZF | kFlagSF, "sahf" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::MovA), 0, 0, "mov_rax_r8" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::MovB), 0, 0, "mov_r9_rbx" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::MovzxByte), 0, 0, "movzx_byte" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::MovzxWord), 0, 0, "movzx_word" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::MovsxByte), 0, 0, "movsx_byte" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::MovsxWord), 0, 0, "movsx_word" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Movsxd), 0, 0, "movsxd_dword" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Lea2), 0, 0, "lea_scale2" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Lea4), 0, 0, "lea_scale4" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Cbw), 0, 0, "cbw" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Cwde), 0, 0, "cwde" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Cdqe), 0, 0, "cdqe" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Cwd), 0, 0, "cwd" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Cdq), 0, 0, "cdq" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::Cqo), 0, 0, "cqo" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::XchgRegReg), 0, 0, "xchg_r8_r9" });
    specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(MoveProgram::XchgRaxR8), 0, 0, "xchg_rax_r8" });
    if (features.movbe) {
        for (const MovbeCaseSpec& movbe_case : kMovbeCases) {
            specs.push_back({ Family::MoveOps, static_cast<std::uint32_t>(movbe_case.op), 0, 0, movbe_case.name });
        }
    }
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::MovRoundtrip), 0, 0, "mov_mem_roundtrip" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::AddMem), 0, kStatusMask, "add_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::SubMem), 0, kStatusMask, "sub_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::AndMem), 0, kLogicMask, "and_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::OrMem), 0, kLogicMask, "or_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::XorMem), 0, kLogicMask, "xor_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::XaddMem), 0, kStatusMask, "xadd_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::CmpxchgMem), 0, kStatusMask, "cmpxchg_mem" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::XaddReg), 0, kStatusMask, "xadd_reg_r8_r9" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::CmpxchgRegEqual), 0, kStatusMask, "cmpxchg_reg_equal" });
    specs.push_back({ Family::MemoryOps, static_cast<std::uint32_t>(MemoryProgram::CmpxchgRegNotEqual), 0, kStatusMask, "cmpxchg_reg_not_equal" });
    specs.push_back({ Family::StackOps, static_cast<std::uint32_t>(StackProgram::PushPopPair), 0, 0, "push_pop_pair" });
    specs.push_back({ Family::StackOps, static_cast<std::uint32_t>(StackProgram::PushPopFlags), 0, 0, "pushf_pop_lahf" });
    specs.push_back({ Family::StackOps, static_cast<std::uint32_t>(StackProgram::EnterLeave), 0, 0, "enter_leave" });
    specs.push_back({ Family::StackOps, static_cast<std::uint32_t>(StackProgram::PushChain), 0, 0, "push_chain" });
    specs.push_back({ Family::StackOps, static_cast<std::uint32_t>(StackProgram::PushImm8Pop), 0, 0, "push_imm8_pop" });
    specs.push_back({ Family::StackOps, static_cast<std::uint32_t>(StackProgram::PushImm32Pop), 0, 0, "push_imm32_pop" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Movsb), 0, 0, "movsb" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Movsw), 0, 0, "movsw" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Movsd), 0, 0, "movsd" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Movsq), 0, 0, "movsq" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::RepMovsb), 0, 0, "rep_movsb" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::RepMovsw), 0, 0, "rep_movsw" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Lodsb), 0, 0, "lodsb" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Lodsq), 0, 0, "lodsq" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Stosb), 0, 0, "stosb" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Stosq), 0, 0, "stosq" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Cmpsb), 0, kStatusMask, "cmpsb" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Scasb), 0, kStatusMask, "scasb" });
    specs.push_back({ Family::StringOps, static_cast<std::uint32_t>(StringProgram::Xlat), 0, 0, "xlat" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Paddq), 0, 0, "paddq" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pxor), 0, 0, "pxor" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pand), 0, 0, "pand" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Por), 0, 0, "por" });
    if (features.sha) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Sha256Rnds2), 0, 0, "sha256rnds2_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Sha256Rnds2), 1, 0, "sha256rnds2_mem" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Sha256Msg1), 0, 0, "sha256msg1_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Sha256Msg1), 1, 0, "sha256msg1_mem" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Sha256Msg2), 0, 0, "sha256msg2_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Sha256Msg2), 1, 0, "sha256msg2_mem" });
    }
    if (features.sse41) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pblendw), 0, 0, "pblendw_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pblendw), 1, 0, "pblendw_mem" });
    }
    if (features.avx) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vpblendw128), 0, 0, "vpblendw_xmm_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vpblendw128), 1, 0, "vpblendw_xmm_mem" });
    }
    if (features.avx2) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vpblendw256), 0, 0, "vpblendw_ymm_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vpblendw256), 1, 0, "vpblendw_ymm_mem" });
    }
    if (features.avx) {
        for (std::size_t index = 0; index < kVinsertf128Immediates.size(); ++index) {
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vinsertf128), static_cast<std::uint32_t>(index), 0, "vinsertf128_reg_imm" + std::to_string(kVinsertf128Immediates[index]) });
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vinsertf128), static_cast<std::uint32_t>(index + kVinsertf128Immediates.size()), 0, "vinsertf128_mem_imm" + std::to_string(kVinsertf128Immediates[index]) });
        }
    }
    if (features.fma) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vfmadd132sd), 0, 0, "vfmadd132sd_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vfmadd132sd), 1, 0, "vfmadd132sd_mem" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vfmadd213sd), 0, 0, "vfmadd213sd_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vfmadd213sd), 1, 0, "vfmadd213sd_mem" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vfmadd231sd), 0, 0, "vfmadd231sd_reg" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Vfmadd231sd), 1, 0, "vfmadd231sd_mem" });
    }
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovdMmReg), 0, 0, "movd_mm_reg" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovqMmReg), 0, 0, "movq_mm_reg" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovdMmMem), 0, 0, "movd_mm_mem" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovqMmMem), 0, 0, "movq_mm_mem" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovdXmmReg), 0, 0, "movd_xmm_reg" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovqXmmReg), 0, 0, "movq_xmm_reg" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovdXmmMem), 0, 0, "movd_xmm_mem" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovqXmmMem), 0, 0, "movq_xmm_mem" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Punpcklbw), 0, 0, "punpcklbw" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Punpcklwd), 0, 0, "punpcklwd" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Punpckldq), 0, 0, "punpckldq" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Punpcklqdq), 0, 0, "punpcklqdq" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pcmpeqb), 0, 0, "pcmpeqb" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pcmpeqw), 0, 0, "pcmpeqw" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pcmpeqd), 0, 0, "pcmpeqd" });
    for (std::size_t index = 0; index < kPshufdImmediates.size(); ++index) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pshufd), static_cast<std::uint32_t>(index), 0, "pshufd_reg_imm" + std::to_string(kPshufdImmediates[index]) });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pshufd), static_cast<std::uint32_t>(index + kPshufdImmediates.size()), 0, "pshufd_mem_imm" + std::to_string(kPshufdImmediates[index]) });
    }
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pslldq), 0, 0, "pslldq" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Psrldq), 0, 0, "psrldq" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::PaddqPxor), 0, 0, "paddq_pxor" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Movdqu), 0, 0, "movdqu_roundtrip" });
    specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::MovapsMemDisp8Neg), 0, 0, "movaps_mem_disp8_neg_xmm6" });
    if (features.ssse3) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pshufb), 0, 0, "pshufb" });
    }
    if (features.aes) {
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Aesenc), 0, 0, "aesenc" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Aesenclast), 0, 0, "aesenclast" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Aesdec), 0, 0, "aesdec" });
        specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Aesdeclast), 0, 0, "aesdeclast" });
        for (std::size_t index = 0; index < kAesKeygenAssistImmediates.size(); ++index) {
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Aeskeygenassist), static_cast<std::uint32_t>(index), 0, "aeskeygenassist_imm" + std::to_string(kAesKeygenAssistImmediates[index]) });
        }
    }
    if (features.sse41) {
        for (std::size_t index = 0; index < kRoundImmediates.size(); ++index) {
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Roundsd), static_cast<std::uint32_t>(index), 0, "roundsd_imm" + std::to_string(kRoundImmediates[index]) });
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Roundss), static_cast<std::uint32_t>(index), 0, "roundss_imm" + std::to_string(kRoundImmediates[index]) });
        }
    }
    if (features.sse42) {
        for (std::size_t index = 0; index < kPcmpestriImmediates.size(); ++index) {
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pcmpestrm), static_cast<std::uint32_t>(index), kPcmpstrMask, "pcmpestrm_imm" + std::to_string(kPcmpestriImmediates[index]) });
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pcmpestri), static_cast<std::uint32_t>(index), kPcmpstrMask, "pcmpestri_imm" + std::to_string(kPcmpestriImmediates[index]) });
            specs.push_back({ Family::VectorOps, static_cast<std::uint32_t>(VectorProgram::Pcmpistrm), static_cast<std::uint32_t>(index), kPcmpstrMask, "pcmpistrm_imm" + std::to_string(kPcmpestriImmediates[index]) });
        }
    }
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::JmpSkip), 0, kStatusMask, "jmp_skip" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::CallAdd), 0, kStatusMask, "call_add" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::CallXor), 0, kLogicMask, "call_xor" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::CallLea), 0, 0, "call_lea" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::Loopnz), 0, kStatusMask, "loopnz_taken" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::Loopz), 0, kStatusMask, "loopz_taken" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::Loop), 0, kStatusMask, "loop_taken" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::Nop), 0, 0, "nop" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::NopModrm), 0, 0, "nop_modrm" });
    specs.push_back({ Family::ControlFlow, static_cast<std::uint32_t>(ControlProgram::Pause), 0, 0, "pause" });
    return specs;
}

inline BuiltCase build_case(const ProgramSpec& spec, std::uint64_t seed) {
    BuiltCase built{};
    built.spec = spec;
    built.seed = seed;
    built.initial_context = make_initial_context(seed);
    auto data = make_initial_data(seed);

    CodeBuilder code;
    const CodeBuilder::Label slot0 = code.make_label();
    const CodeBuilder::Label slot1 = code.make_label();
    const CodeBuilder::Label slot2 = code.make_label();
    const CodeBuilder::Label slot3 = code.make_label();
    const CodeBuilder::Label buffer0 = code.make_label();
    const CodeBuilder::Label buffer1 = code.make_label();
    const CodeBuilder::Label label_true = code.make_label();
    const CodeBuilder::Label label_false = code.make_label();
    const CodeBuilder::Label label_end = code.make_label();
    const CodeBuilder::Label helper = code.make_label();
    const CodeBuilder::Label after_helper = code.make_label();

    switch (spec.family) {
    case Family::BinaryRegReg: {
        const auto op = static_cast<BinaryOp>(spec.op);
        const PairSpec pair = kBinaryPairs[spec.variant % kBinaryPairs.size()];
        code.binary_reg_reg(op, pair.dst, pair.src);
        break;
    }
    case Family::BinaryRegImm: {
        const auto op = static_cast<BinaryOp>(spec.op);
        const Reg reg = kCoreRegs[spec.variant % kCoreRegs.size()].reg;
        std::uint32_t imm = narrow32(seeded(seed, 0x600));
        if (imm == 0) imm = 1;
        code.binary_reg_imm(op, reg, imm);
        break;
    }
    case Family::UnaryReg: {
        const auto op = static_cast<UnaryOp>(spec.op);
        const Reg reg = kUnaryRegs[spec.variant % kUnaryRegs.size()].reg;
        if (op == UnaryOp::Div || op == UnaryOp::Idiv) {
            built.initial_context.regs[static_cast<std::size_t>(Reg::RAX)] = 0x12345ull;
            built.initial_context.regs[static_cast<std::size_t>(Reg::RDX)] = 0;
            if (reg != Reg::RAX) {
                built.initial_context.regs[static_cast<std::size_t>(reg)] = 0x123ull;
            }
        }
        code.unary_reg(op, reg);
        break;
    }
    case Family::ShiftImm: {
        const auto op = static_cast<ShiftOp>(spec.op);
        const Reg reg = kCoreRegs[spec.variant % kCoreRegs.size()].reg;
        code.shift_imm(op, reg, 1);
        break;
    }
    case Family::ShiftCl: {
        const auto op = static_cast<ShiftOp>(spec.op);
        const Reg reg = kCoreRegs[spec.variant % kCoreRegs.size()].reg;
        built.initial_context.regs[static_cast<std::size_t>(Reg::RCX)] = 1;
        code.shift_cl(op, reg);
        break;
    }
    case Family::BitOps: {
        const auto op = static_cast<BitOp>(spec.op);
        if (const Crc32CaseSpec* crc32_case = find_crc32_case(op)) {
            code.crc32_case(*crc32_case, slot0);
            break;
        }
        switch (op) {
        case BitOp::BtImm:
            code.bt_reg_imm(Reg::RAX, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtReg:
            built.initial_context.regs[static_cast<std::size_t>(Reg::RCX)] = seed % 63;
            code.bt_reg_reg(Reg::R8, Reg::RCX);
            break;
        case BitOp::BtMemImm:
            code.bit_mem_imm(4, buffer0, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtsImm:
            code.bit_reg_imm(5, Reg::R10, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtsReg:
            built.initial_context.regs[static_cast<std::size_t>(Reg::RCX)] = seed % 63;
            code.bit_reg_reg(0xAB, Reg::R11, Reg::RCX);
            break;
        case BitOp::BtsMemImm:
            code.bit_mem_imm(5, buffer0, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtrImm:
            code.bit_reg_imm(6, Reg::R12, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtrReg:
            built.initial_context.regs[static_cast<std::size_t>(Reg::RCX)] = seed % 63;
            code.bit_reg_reg(0xB3, Reg::R13, Reg::RCX);
            break;
        case BitOp::BtrMemImm:
            code.bit_mem_imm(6, buffer0, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtcImm:
            code.bit_reg_imm(7, Reg::R14, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::BtcReg:
            built.initial_context.regs[static_cast<std::size_t>(Reg::RCX)] = seed % 63;
            code.bit_reg_reg(0xBB, Reg::R15, Reg::RCX);
            break;
        case BitOp::BtcMemImm:
            code.bit_mem_imm(7, buffer0, static_cast<std::uint8_t>(seed % 63));
            break;
        case BitOp::Bsf:
            built.initial_context.regs[static_cast<std::size_t>(Reg::RBX)] |= 1;
            code.bsf(Reg::RDX, Reg::RBX);
            break;
        case BitOp::Bsr:
            built.initial_context.regs[static_cast<std::size_t>(Reg::R10)] |= 1;
            code.bsr(Reg::R9, Reg::R10);
            break;
        case BitOp::Popcnt:
            code.popcnt(Reg::RDX, Reg::RBX);
            break;
        case BitOp::Bswap:
            code.bswap(Reg::R11);
            break;
        case BitOp::BsfAlt:
            built.initial_context.regs[static_cast<std::size_t>(Reg::R10)] |= 1;
            code.bsf(Reg::R9, Reg::R10);
            break;
        case BitOp::BswapAlt:
            code.bswap(Reg::RAX);
            break;
        case BitOp::Tzcnt:
            code.tzcnt(Reg::RDX, Reg::RBX);
            break;
        case BitOp::Lzcnt:
            code.lzcnt(Reg::R9, Reg::R10);
            break;
        case BitOp::ImulRegReg:
            code.imul_reg_reg(Reg::R10, Reg::R11);
            break;
        case BitOp::ImulRegImm8:
            code.imul_reg_reg_imm8(Reg::R12, Reg::R13, 7);
            break;
        case BitOp::ImulRegImm32:
            code.imul_reg_reg_imm32(Reg::R14, Reg::R15, 0x12345u);
            break;
        }
        break;
    }
    case Family::FlagOps: {
        const std::uint8_t cc = condition_variant_code(spec.variant);
        const auto program = static_cast<FlagProgram>(spec.op);
        if (program == FlagProgram::Setcc) {
            built.initial_context.rflags = forced_condition_flags(cc, condition_variant_expected_true(spec.variant));
            code.setcc(cc, Reg::RAX);
        }
        else if (program == FlagProgram::Cmovcc) {
            built.initial_context.rflags = forced_condition_flags(cc, condition_variant_expected_true(spec.variant));
            code.cmovcc(cc, Reg::R10, Reg::R11);
        }
        else if (program == FlagProgram::Jcc) {
            built.initial_context.rflags = forced_condition_flags(cc, condition_variant_expected_true(spec.variant));
            code.jcc32(cc, label_true);
            code.mov_reg_reg(Reg::R12, Reg::R13);
            code.jmp32(label_end);
            code.mark(label_true);
            code.mov_reg_reg(Reg::R12, Reg::R14);
            code.mark(label_end);
        }
        else if (program == FlagProgram::Cmc) {
            code.cmc();
        }
        else if (program == FlagProgram::Clc) {
            code.clc();
        }
        else if (program == FlagProgram::Stc) {
            code.stc();
        }
        else if (program == FlagProgram::Cld) {
            code.std_();
            code.cld();
        }
        else if (program == FlagProgram::Lahf) {
            code.lahf();
        }
        else {
            code.sahf();
        }
        break;
    }
    case Family::MoveOps: {
        const auto program = static_cast<MoveProgram>(spec.op);
        if (const MovbeCaseSpec* movbe_case = find_movbe_case(program)) {
            code.movbe_case(*movbe_case, slot0, buffer0);
            break;
        }
        switch (program) {
        case MoveProgram::MovA:
            code.mov_reg_reg(Reg::RAX, Reg::R8);
            break;
        case MoveProgram::MovB:
            code.mov_reg_reg(Reg::R9, Reg::RBX);
            break;
        case MoveProgram::MovzxByte:
            code.movzx_byte(Reg::RAX, slot0);
            break;
        case MoveProgram::MovzxWord:
            code.movzx_word(Reg::R10, slot0);
            break;
        case MoveProgram::MovsxByte:
            data[0] |= 0x80u;
            code.movsx_byte(Reg::R11, slot0);
            break;
        case MoveProgram::MovsxWord:
            data[0] = 0x34u;
            data[1] = 0xF2u;
            code.movsx_word(Reg::R12, slot0);
            break;
        case MoveProgram::Movsxd:
            data[0] = 0x78u;
            data[1] = 0x56u;
            data[2] = 0x34u;
            data[3] = 0xF2u;
            code.movsxd_dword(Reg::R13, slot0);
            break;
        case MoveProgram::Lea2:
            code.lea_scaled(Reg::R14, Reg::RBX, Reg::RSI, 1, 0x24);
            break;
        case MoveProgram::Lea4:
            code.lea_scaled(Reg::R15, Reg::R8, Reg::R9, 2, 0x18);
            break;
        case MoveProgram::Cbw:
            code.cbw();
            break;
        case MoveProgram::Cwde:
            code.cwde();
            break;
        case MoveProgram::Cdqe:
            code.cdqe();
            break;
        case MoveProgram::Cwd:
            code.cwd();
            break;
        case MoveProgram::Cdq:
            code.cdq();
            break;
        case MoveProgram::Cqo:
            code.cqo();
            break;
        case MoveProgram::XchgRegReg:
            code.xchg_reg_reg(Reg::R8, Reg::R9);
            break;
        case MoveProgram::XchgRaxR8:
            code.xchg_rax_reg(Reg::R8);
            break;
        }
        break;
    }
    case Family::MemoryOps: {
        switch (static_cast<MemoryProgram>(spec.op)) {
        case MemoryProgram::MovRoundtrip:
            code.mov_mem_reg(buffer0, Reg::RAX);
            code.mov_reg_mem(Reg::RBX, buffer0);
            break;
        case MemoryProgram::AddMem:
            code.binary_mem_reg(BinaryOp::Add, buffer0, Reg::R8);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::SubMem:
            code.binary_mem_reg(BinaryOp::Sub, buffer0, Reg::R9);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::AndMem:
            code.binary_mem_reg(BinaryOp::And, buffer0, Reg::R10);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::OrMem:
            code.binary_mem_reg(BinaryOp::Or, buffer0, Reg::R11);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::XorMem:
            code.binary_mem_reg(BinaryOp::Xor, buffer0, Reg::R12);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::XaddMem:
            code.xadd_mem_reg(buffer0, Reg::R13);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::CmpxchgMem:
            code.cmpxchg_mem_reg(buffer0, Reg::R14);
            code.mov_reg_mem(Reg::RDX, buffer0);
            break;
        case MemoryProgram::XaddReg:
            code.xadd_reg_reg(Reg::R8, Reg::R9);
            break;
        case MemoryProgram::CmpxchgRegEqual:
            built.initial_context.regs[static_cast<std::size_t>(Reg::R8)] = seeded(seed, 0x8C0);
            built.initial_context.regs[static_cast<std::size_t>(Reg::RAX)] = built.initial_context.regs[static_cast<std::size_t>(Reg::R8)];
            code.cmpxchg_reg_reg(Reg::R8, Reg::R9);
            break;
        case MemoryProgram::CmpxchgRegNotEqual:
            built.initial_context.regs[static_cast<std::size_t>(Reg::R8)] = seeded(seed, 0x8C1);
            built.initial_context.regs[static_cast<std::size_t>(Reg::RAX)] = built.initial_context.regs[static_cast<std::size_t>(Reg::R8)] ^ 0x5A5A5A5A5A5A5A5Aull;
            code.cmpxchg_reg_reg(Reg::R8, Reg::R9);
            break;
        }
        break;
    }
    case Family::StackOps: {
        switch (static_cast<StackProgram>(spec.op)) {
        case StackProgram::PushPopPair:
            code.push_reg(Reg::RAX);
            code.push_reg(Reg::RBX);
            code.pop_reg(Reg::R10);
            code.pop_reg(Reg::R11);
            break;
        case StackProgram::PushPopFlags:
            code.pushf();
            code.pop_reg(Reg::RAX);
            code.lahf();
            break;
        case StackProgram::EnterLeave:
            code.enter(0x20);
            code.mov_reg_reg(Reg::R12, Reg::RBP);
            code.leave();
            break;
        case StackProgram::PushChain:
            code.push_reg(Reg::R8);
            code.push_reg(Reg::R9);
            code.push_reg(Reg::R10);
            code.pop_reg(Reg::R11);
            code.pop_reg(Reg::R12);
            code.pop_reg(Reg::R13);
            break;
        case StackProgram::PushImm8Pop:
            code.push_imm8(narrow8(seeded(seed, 0x7A0)));
            code.pop_reg(Reg::R14);
            break;
        case StackProgram::PushImm32Pop:
            code.push_imm32(narrow32(seeded(seed, 0x7A1)));
            code.pop_reg(Reg::R15);
            break;
        }
        break;
    }
    case Family::StringOps: {
        const auto program = static_cast<StringProgram>(spec.op);
        code.lea_rip(Reg::RSI, buffer0);
        code.lea_rip(Reg::RDI, buffer1);
        code.mov_r32_imm(Reg::RCX, 4);
        switch (program) {
        case StringProgram::Movsb:
            code.movsb();
            break;
        case StringProgram::Movsw:
            code.movsw();
            break;
        case StringProgram::Movsd:
            code.movsd();
            break;
        case StringProgram::Movsq:
            code.movsq();
            break;
        case StringProgram::RepMovsb:
            code.mov_r32_imm(Reg::RCX, 12);
            code.rep();
            code.movsb();
            break;
        case StringProgram::RepMovsw:
            code.mov_r32_imm(Reg::RCX, 6);
            code.rep();
            code.movsw();
            break;
        case StringProgram::Lodsb:
            code.lodsb();
            break;
        case StringProgram::Lodsq:
            code.lodsq();
            break;
        case StringProgram::Stosb:
            code.mov_r8_imm(Reg::RAX, narrow8(seeded(seed, 0x700)));
            code.stosb();
            break;
        case StringProgram::Stosq:
            code.stosq();
            break;
        case StringProgram::Cmpsb:
            code.cmpsb();
            break;
        case StringProgram::Scasb:
            code.mov_r8_imm(Reg::RAX, data[kSlotSize * 4 + 5]);
            code.scasb();
            break;
        case StringProgram::Xlat:
            code.lea_rip(Reg::RBX, buffer0);
            code.mov_r8_imm(Reg::RAX, 5);
            code.xlat();
            break;
        }
        break;
    }
    case Family::VectorOps: {
        const auto program = static_cast<VectorProgram>(spec.op);
        code.movdqu_load(Reg::RAX, slot0);
        code.movdqu_load(Reg::RCX, slot1);
        switch (program) {
        case VectorProgram::Paddq:
            code.paddq(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pxor:
            code.pxor(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pand:
            code.pand(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Por:
            code.por(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Sha256Rnds2:
            if (spec.variant == 0) {
                code.movdqu_load(Reg::RDX, slot2);
                code.sha256rnds2(Reg::RCX, Reg::RDX);
            }
            else {
                code.sha256rnds2_mem(Reg::RCX, slot2);
            }
            code.movdqu_store(buffer0, Reg::RCX);
            break;
        case VectorProgram::Sha256Msg1:
            if (spec.variant == 0) {
                code.sha256msg1(Reg::RAX, Reg::RCX);
            }
            else {
                code.sha256msg1_mem(Reg::RAX, slot2);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Sha256Msg2:
            if (spec.variant == 0) {
                code.sha256msg2(Reg::RAX, Reg::RCX);
            }
            else {
                code.sha256msg2_mem(Reg::RAX, slot2);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pblendw:
            if (spec.variant == 0) {
                code.pblendw(Reg::RAX, Reg::RCX, 0x53);
            }
            else {
                code.pblendw_mem(Reg::RAX, slot2, 0xCA);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Vpblendw128:
            if (spec.variant == 0) {
                code.vpblendw_xmm(Reg::RDX, Reg::RAX, Reg::RCX, 0x53);
            }
            else {
                code.vpblendw_xmm_mem(Reg::RDX, Reg::RAX, slot2, 0xCA);
            }
            code.movdqu_store(buffer0, Reg::RDX);
            break;
        case VectorProgram::Vpblendw256:
            code.vmovups_ymm_load(Reg::RAX, slot0);
            if (spec.variant == 0) {
                code.vmovups_ymm_load(Reg::RCX, slot2);
                code.vpblendw_ymm(Reg::RDX, Reg::RAX, Reg::RCX, 0x53);
            }
            else {
                code.vpblendw_ymm_mem(Reg::RDX, Reg::RAX, buffer1, 0xCA);
            }
            code.vmovups_ymm_store(buffer0, Reg::RDX);
            break;
        case VectorProgram::Vinsertf128: {
            const std::uint8_t imm8 = kVinsertf128Immediates[spec.variant % kVinsertf128Immediates.size()];
            code.vmovups_ymm_load(Reg::RAX, slot0);
            if (spec.variant < kVinsertf128Immediates.size()) {
                code.movdqu_load(Reg::RCX, slot2);
                code.vinsertf128(Reg::RDX, Reg::RAX, Reg::RCX, imm8);
            }
            else {
                code.vinsertf128_mem(Reg::RDX, Reg::RAX, slot2, imm8);
            }
            code.vmovups_ymm_store(buffer0, Reg::RDX);
            break;
        }
        case VectorProgram::Vfmadd132sd:
            if (spec.variant == 0) {
                code.movdqu_load(Reg::RDX, slot2);
                code.vfmadd132sd(Reg::RAX, Reg::RCX, Reg::RDX);
            }
            else {
                code.vfmadd132sd_mem(Reg::RAX, Reg::RCX, slot2);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Vfmadd213sd:
            if (spec.variant == 0) {
                code.movdqu_load(Reg::RDX, slot2);
                code.vfmadd213sd(Reg::RAX, Reg::RCX, Reg::RDX);
            }
            else {
                code.vfmadd213sd_mem(Reg::RAX, Reg::RCX, slot2);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Vfmadd231sd:
            if (spec.variant == 0) {
                code.movdqu_load(Reg::RDX, slot2);
                code.vfmadd231sd(Reg::RAX, Reg::RCX, Reg::RDX);
            }
            else {
                code.vfmadd231sd_mem(Reg::RAX, Reg::RCX, slot2);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::MovdMmReg:
            code.movd_mm_reg(Reg::RAX, Reg::R8);
            code.movd_mem_mm(buffer0, Reg::RAX);
            break;
        case VectorProgram::MovqMmReg:
            code.movq_mm_reg(Reg::RAX, Reg::R9);
            code.movq_mem_mm(buffer0, Reg::RAX);
            break;
        case VectorProgram::MovdMmMem:
            code.movd_mm_mem(Reg::RAX, slot0);
            code.movd_mem_mm(buffer0, Reg::RAX);
            break;
        case VectorProgram::MovqMmMem:
            code.movq_mm_mem(Reg::RAX, slot0);
            code.movq_mem_mm(buffer0, Reg::RAX);
            break;
        case VectorProgram::MovdXmmReg:
            code.movd_xmm_reg(Reg::RAX, Reg::R10);
            code.movdqu_store(buffer0, Reg::RAX);
            code.movd_mem_xmm(buffer1, Reg::RAX);
            break;
        case VectorProgram::MovqXmmReg:
            code.movq_xmm_reg(Reg::RAX, Reg::R11);
            code.movdqu_store(buffer0, Reg::RAX);
            code.movq_mem_xmm(buffer1, Reg::RAX);
            break;
        case VectorProgram::MovdXmmMem:
            code.movd_xmm_mem(Reg::RAX, slot0);
            code.movdqu_store(buffer0, Reg::RAX);
            code.movd_mem_xmm(buffer1, Reg::RAX);
            break;
        case VectorProgram::MovqXmmMem:
            code.movq_xmm_mem(Reg::RAX, slot0);
            code.movdqu_store(buffer0, Reg::RAX);
            code.movq_mem_xmm(buffer1, Reg::RAX);
            break;
        case VectorProgram::Punpcklbw:
            code.punpcklbw(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Punpcklwd:
            code.punpcklwd(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Punpckldq:
            code.punpckldq(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Punpcklqdq:
            code.punpcklqdq(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pcmpeqb:
            code.pcmpeqb(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pcmpeqw:
            code.pcmpeqw(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pcmpeqd:
            code.pcmpeqd(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pshufd: {
            const std::size_t imm_index = spec.variant % kPshufdImmediates.size();
            const std::uint8_t imm8 = kPshufdImmediates[imm_index];
            if (spec.variant < kPshufdImmediates.size()) {
                code.pshufd(Reg::RAX, Reg::RAX, imm8);
            }
            else {
                code.pshufd_mem(Reg::RAX, slot1, imm8);
            }
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        }
        case VectorProgram::Pslldq:
            code.pslldq(Reg::RAX, 4);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Psrldq:
            code.psrldq(Reg::RAX, 5);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::PaddqPxor:
            code.paddq(Reg::RAX, Reg::RCX);
            code.pxor(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Movdqu:
            code.movdqu_store(buffer0, Reg::RAX);
            code.movdqu_load(Reg::RDX, buffer0);
            code.movdqu_store(buffer1, Reg::RDX);
            break;
        case VectorProgram::MovapsMemDisp8Neg:
            code.lea_rip(Reg::RAX, buffer0);
            code.binary_reg_imm(BinaryOp::Add, Reg::RAX, 0x58);
            code.emit8(0x0F);
            code.emit8(0x29);
            code.emit8(0x70);
            code.emit8(0xA8);
            break;
        case VectorProgram::Pshufb:
            code.pshufb(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Aesenc:
            code.aesenc(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Aesenclast:
            code.aesenclast(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Aesdec:
            code.aesdec(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Aesdeclast:
            code.aesdeclast(Reg::RAX, Reg::RCX);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Aeskeygenassist:
            code.aeskeygenassist(Reg::RAX, Reg::RCX, kAesKeygenAssistImmediates[spec.variant % kAesKeygenAssistImmediates.size()]);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Roundsd:
            code.roundsd(Reg::RAX, Reg::RCX, kRoundImmediates[spec.variant % kRoundImmediates.size()]);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Roundss:
            code.roundss(Reg::RAX, Reg::RCX, kRoundImmediates[spec.variant % kRoundImmediates.size()]);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        case VectorProgram::Pcmpestrm: {
            const std::uint8_t imm8 = kPcmpestriImmediates[spec.variant % kPcmpestriImmediates.size()];
            const std::int32_t max_length = (imm8 & 0x01u) != 0 ? 8 : 16;
            const std::int32_t length_a = static_cast<std::int32_t>(seeded(seed, 0x820) % static_cast<std::uint64_t>(max_length * 4 + 1)) - max_length * 2;
            const std::int32_t length_b = static_cast<std::int32_t>(seeded(seed, 0x821) % static_cast<std::uint64_t>(max_length * 4 + 1)) - max_length * 2;
            built.initial_context.regs[static_cast<std::size_t>(Reg::RAX)] = static_cast<std::uint64_t>(static_cast<std::int64_t>(length_a));
            built.initial_context.regs[static_cast<std::size_t>(Reg::RDX)] = static_cast<std::uint64_t>(static_cast<std::int64_t>(length_b));
            code.pcmpestrm(Reg::RAX, Reg::RCX, imm8);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        }
        case VectorProgram::Pcmpestri: {
            const std::uint8_t imm8 = kPcmpestriImmediates[spec.variant % kPcmpestriImmediates.size()];
            const std::int32_t max_length = (imm8 & 0x01u) != 0 ? 8 : 16;
            const std::int32_t length_a = static_cast<std::int32_t>(seeded(seed, 0x830) % static_cast<std::uint64_t>(max_length * 4 + 1)) - max_length * 2;
            const std::int32_t length_b = static_cast<std::int32_t>(seeded(seed, 0x831) % static_cast<std::uint64_t>(max_length * 4 + 1)) - max_length * 2;
            built.initial_context.regs[static_cast<std::size_t>(Reg::RAX)] = static_cast<std::uint64_t>(static_cast<std::int64_t>(length_a));
            built.initial_context.regs[static_cast<std::size_t>(Reg::RDX)] = static_cast<std::uint64_t>(static_cast<std::int64_t>(length_b));
            code.pcmpestri(Reg::RAX, Reg::RCX, imm8);
            break;
        }
        case VectorProgram::Pcmpistrm: {
            const std::uint8_t imm8 = kPcmpestriImmediates[spec.variant % kPcmpestriImmediates.size()];
            code.pcmpistrm(Reg::RAX, Reg::RCX, imm8);
            code.movdqu_store(buffer0, Reg::RAX);
            break;
        }
        }
        break;
    }
    case Family::ControlFlow: {
        switch (static_cast<ControlProgram>(spec.op)) {
        case ControlProgram::JmpSkip:
            code.jmp32(label_end);
            code.mov_reg_reg(Reg::RAX, Reg::R15);
            code.mark(label_end);
            code.binary_reg_reg(BinaryOp::Add, Reg::RAX, Reg::RBX);
            break;
        case ControlProgram::CallAdd:
            code.call32(helper);
            code.mov_reg_reg(Reg::R8, Reg::RAX);
            code.jmp32(after_helper);
            code.mark(helper);
            code.binary_reg_reg(BinaryOp::Add, Reg::RAX, Reg::RBX);
            code.ret();
            code.mark(after_helper);
            break;
        case ControlProgram::CallXor:
            code.call32(helper);
            code.mov_reg_reg(Reg::R9, Reg::R10);
            code.jmp32(after_helper);
            code.mark(helper);
            code.binary_reg_reg(BinaryOp::Xor, Reg::R10, Reg::R11);
            code.ret();
            code.mark(after_helper);
            break;
        case ControlProgram::CallLea:
            code.call32(helper);
            code.mov_reg_reg(Reg::R12, Reg::R14);
            code.jmp32(after_helper);
            code.mark(helper);
            code.lea_scaled(Reg::R14, Reg::RAX, Reg::RBX, 1, 0x10);
            code.ret();
            code.mark(after_helper);
            break;
        case ControlProgram::Loopnz:
            code.mov_r32_imm(Reg::RAX, 1);
            code.mov_r32_imm(Reg::RBX, 2);
            code.mov_r32_imm(Reg::RCX, 2);
            code.binary_reg_reg(BinaryOp::Cmp, Reg::RAX, Reg::RBX);
            code.loopcc(0xE0, label_true);
            code.mov_reg_reg(Reg::R12, Reg::R13);
            code.jmp32(label_end);
            code.mark(label_true);
            code.mov_reg_reg(Reg::R12, Reg::R14);
            code.mark(label_end);
            break;
        case ControlProgram::Loopz:
            code.mov_r32_imm(Reg::RAX, 1);
            code.mov_r32_imm(Reg::RBX, 1);
            code.mov_r32_imm(Reg::RCX, 2);
            code.binary_reg_reg(BinaryOp::Cmp, Reg::RAX, Reg::RBX);
            code.loopcc(0xE1, label_true);
            code.mov_reg_reg(Reg::R12, Reg::R13);
            code.jmp32(label_end);
            code.mark(label_true);
            code.mov_reg_reg(Reg::R12, Reg::R14);
            code.mark(label_end);
            break;
        case ControlProgram::Loop:
            code.mov_r32_imm(Reg::RCX, 2);
            code.loopcc(0xE2, label_true);
            code.mov_reg_reg(Reg::R12, Reg::R13);
            code.jmp32(label_end);
            code.mark(label_true);
            code.mov_reg_reg(Reg::R12, Reg::R14);
            code.mark(label_end);
            break;
        case ControlProgram::Nop:
            code.nop1();
            break;
        case ControlProgram::NopModrm:
            code.nop_modrm_reg();
            break;
        case ControlProgram::Pause:
            code.pause();
            break;
        }
        break;
    }
    }

    code.ret();
    code.align(16);
    built.data_offset = static_cast<std::uint32_t>(code.offset());
    code.mark(slot0);
    code.emit_bytes(data.data(), kSlotSize);
    code.mark(slot1);
    code.emit_bytes(data.data() + kSlotSize, kSlotSize);
    code.mark(slot2);
    code.emit_bytes(data.data() + kSlotSize * 2, kSlotSize);
    code.mark(slot3);
    code.emit_bytes(data.data() + kSlotSize * 3, kSlotSize);
    code.mark(buffer0);
    code.emit_bytes(data.data() + kSlotSize * 4, kBufferSize);
    code.mark(buffer1);
    code.emit_bytes(data.data() + kSlotSize * 4 + kBufferSize, kBufferSize);
    if (!code.finalize()) {
        built.image.clear();
        return built;
    }
    built.image = code.bytes();
    return built;
}

inline bool write_engine_reg(cpueaxh_engine* engine, int reg, std::uint64_t value) {
    return cpueaxh_reg_write(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool read_engine_reg(cpueaxh_engine* engine, int reg, std::uint64_t& value) {
    return cpueaxh_reg_read(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool write_engine_reg32(cpueaxh_engine* engine, int reg, std::uint32_t value) {
    return cpueaxh_reg_write(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool read_engine_reg32(cpueaxh_engine* engine, int reg, std::uint32_t& value) {
    return cpueaxh_reg_read(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool write_engine_reg16(cpueaxh_engine* engine, int reg, std::uint16_t value) {
    return cpueaxh_reg_write(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool read_engine_reg16(cpueaxh_engine* engine, int reg, std::uint16_t& value) {
    return cpueaxh_reg_read(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool write_engine_xmm(cpueaxh_engine* engine, int reg, const cpueaxh_x86_xmm& value) {
    return cpueaxh_reg_write(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool read_engine_xmm(cpueaxh_engine* engine, int reg, cpueaxh_x86_xmm& value) {
    return cpueaxh_reg_read(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool write_engine_ymm(cpueaxh_engine* engine, int reg, const cpueaxh_x86_ymm& value) {
    return cpueaxh_reg_write(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool read_engine_ymm(cpueaxh_engine* engine, int reg, cpueaxh_x86_ymm& value) {
    return cpueaxh_reg_read(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool write_engine_zmm(cpueaxh_engine* engine, int reg, const cpueaxh_x86_zmm& value) {
    return cpueaxh_reg_write(engine, reg, &value) == CPUEAXH_ERR_OK;
}

inline bool read_engine_zmm(cpueaxh_engine* engine, int reg, cpueaxh_x86_zmm& value) {
    return cpueaxh_reg_read(engine, reg, &value) == CPUEAXH_ERR_OK;
}

} // namespace cpueaxh_test

