#pragma once
// Split from test/demo/framework.hpp: types, options, strict replay helpers, host features, manifests, and shared metadata.
// This header is self-contained and may be included directly; framework.hpp is only an umbrella include.

#define NOMINMAX

#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cpueaxh.hpp"
#include "cpu/def.h"
#include "memory/manager.hpp"

void init_cpu_context(CPU_CONTEXT* ctx, MEMORY_MANAGER* mem_mgr, bool start_in_compat32 = false);
int cpu_step(CPU_CONTEXT* ctx);

constexpr int kCpuStepOk = 0;
constexpr int kCpuStepException = 4;

extern "C" int cpueaxh_test_run_native(cpueaxh_x86_context* context, void* code, void* bridge_block);

namespace cpueaxh_test {

constexpr std::uint64_t kFlagCF = 1ull << 0;
constexpr std::uint64_t kFlagPF = 1ull << 2;
constexpr std::uint64_t kFlagAF = 1ull << 4;
constexpr std::uint64_t kFlagZF = 1ull << 6;
constexpr std::uint64_t kFlagSF = 1ull << 7;
constexpr std::uint64_t kFlagDF = 1ull << 10;
constexpr std::uint64_t kFlagOF = 1ull << 11;
constexpr std::uint64_t kStatusMask = kFlagCF | kFlagPF | kFlagAF | kFlagZF | kFlagSF | kFlagOF;
constexpr std::uint64_t kLogicMask = kFlagCF | kFlagPF | kFlagZF | kFlagSF | kFlagOF;
constexpr std::uint64_t kPcmpstrMask = kFlagCF | kFlagZF | kFlagSF | kFlagOF;
constexpr std::uint64_t kIncDecMask = kFlagPF | kFlagAF | kFlagZF | kFlagSF | kFlagOF;
constexpr std::uint64_t kRotationMask = kFlagCF | kFlagOF;
constexpr std::uint64_t kBitTestMask = kFlagCF;
constexpr std::uint64_t kBitScanMask = kFlagZF;
constexpr std::uint64_t kBitCountMask = kFlagCF | kFlagZF;
constexpr std::uint64_t kSeedCount = 128;
constexpr std::uint64_t kExceptionSeedCount = 128;
constexpr std::uint64_t kHostStackSeedCount = 64;
constexpr std::uint64_t kContextApiSeedCount = 128;
constexpr std::uint64_t kGuestCodeBase = 0x100000;
constexpr std::uint64_t kGuestStackBase = 0x200000;
constexpr std::uint64_t kGuestGdtBase = 0x300000;
constexpr std::size_t kCodePageSize = 0x1000;
constexpr std::size_t kStackSize = 0x4000;
constexpr std::size_t kSlotSize = 16;
constexpr std::size_t kBufferSize = 64;
constexpr std::size_t kDataSize = kSlotSize * 4 + kBufferSize * 2;
constexpr std::uint64_t kInitialMxcsr = 0x1F80;
constexpr std::uint16_t kInitialX87ControlWord = 0x037F;
constexpr std::uint16_t kInitialX87StatusWord = 0x0000;
constexpr std::uint64_t kInitialRspOffset = kStackSize - 0x200;

inline bool expected_x87_pending(std::uint16_t control_word, std::uint16_t status_word) {
    const std::uint16_t status_flags = static_cast<std::uint16_t>(status_word & 0x003Fu);
    const std::uint16_t unmasked = static_cast<std::uint16_t>(status_flags & static_cast<std::uint16_t>(~control_word) & 0x003Fu);
    return unmasked != 0;
}

struct TestBridgeBlock {
    std::uint64_t host_rsp;
    std::uint64_t context_ptr;
    std::uint64_t guest_rsp;
    std::uint64_t saved_stack0;
    std::uint64_t saved_stack1;
    std::uint64_t saved_stack2;
    std::uint64_t guest_rax;
    std::uint64_t guest_r10;
};

// Keep the native runner's hard-coded MASM offsets in sync with the public context layout.
static_assert(offsetof(cpueaxh_x86_context, regs[0]) == 0);
static_assert(offsetof(cpueaxh_x86_context, regs[1]) == 8);
static_assert(offsetof(cpueaxh_x86_context, regs[2]) == 16);
static_assert(offsetof(cpueaxh_x86_context, regs[3]) == 24);
static_assert(offsetof(cpueaxh_x86_context, regs[4]) == 32);
static_assert(offsetof(cpueaxh_x86_context, regs[5]) == 40);
static_assert(offsetof(cpueaxh_x86_context, regs[6]) == 48);
static_assert(offsetof(cpueaxh_x86_context, regs[7]) == 56);
static_assert(offsetof(cpueaxh_x86_context, regs[8]) == 64);
static_assert(offsetof(cpueaxh_x86_context, regs[9]) == 72);
static_assert(offsetof(cpueaxh_x86_context, regs[10]) == 80);
static_assert(offsetof(cpueaxh_x86_context, regs[11]) == 88);
static_assert(offsetof(cpueaxh_x86_context, regs[12]) == 96);
static_assert(offsetof(cpueaxh_x86_context, regs[13]) == 104);
static_assert(offsetof(cpueaxh_x86_context, regs[14]) == 112);
static_assert(offsetof(cpueaxh_x86_context, regs[15]) == 120);
static_assert(offsetof(cpueaxh_x86_context, rip) == 128);
static_assert(offsetof(cpueaxh_x86_context, rflags) == 136);
static_assert(sizeof(cpueaxh_x86_xmm) == 16);
static_assert(offsetof(cpueaxh_x86_context, xmm[0]) == 144);
static_assert(offsetof(cpueaxh_x86_context, xmm[1]) == 160);
static_assert(offsetof(cpueaxh_x86_context, xmm[2]) == 176);
static_assert(offsetof(cpueaxh_x86_context, xmm[3]) == 192);
static_assert(offsetof(cpueaxh_x86_context, xmm[4]) == 208);
static_assert(offsetof(cpueaxh_x86_context, xmm[5]) == 224);
static_assert(offsetof(cpueaxh_x86_context, xmm[6]) == 240);
static_assert(offsetof(cpueaxh_x86_context, xmm[7]) == 256);
static_assert(offsetof(cpueaxh_x86_context, xmm[8]) == 272);
static_assert(offsetof(cpueaxh_x86_context, xmm[9]) == 288);
static_assert(offsetof(cpueaxh_x86_context, xmm[10]) == 304);
static_assert(offsetof(cpueaxh_x86_context, xmm[11]) == 320);
static_assert(offsetof(cpueaxh_x86_context, xmm[12]) == 336);
static_assert(offsetof(cpueaxh_x86_context, xmm[13]) == 352);
static_assert(offsetof(cpueaxh_x86_context, xmm[14]) == 368);
static_assert(offsetof(cpueaxh_x86_context, xmm[15]) == 384);
static_assert(offsetof(cpueaxh_x86_context, mxcsr) == 1296);

inline std::uint64_t mix64(std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

inline std::uint32_t narrow32(std::uint64_t value) {
    return static_cast<std::uint32_t>(value & 0xFFFFFFFFu);
}

inline std::uint8_t narrow8(std::uint64_t value) {
    return static_cast<std::uint8_t>(value & 0xFFu);
}

inline std::uint64_t seeded(std::uint64_t seed, std::uint64_t salt) {
    return mix64(seed + salt * 0x9E3779B97F4A7C15ull + 0xD1B54A32D192ED03ull);
}

inline std::string hex64(std::uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

inline std::string hex8(std::uint8_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(value);
    return stream.str();
}

inline std::string bytes_hex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index != 0) {
            stream << ' ';
        }
        stream << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return stream.str();
}

inline std::string json_escape(const std::string& value) {
    std::ostringstream stream;
    for (const char ch : value) {
        switch (ch) {
        case '\\': stream << "\\\\"; break;
        case '"': stream << "\\\""; break;
        case '\b': stream << "\\b"; break;
        case '\f': stream << "\\f"; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20u) {
                stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(static_cast<unsigned char>(ch));
            }
            else {
                stream << ch;
            }
            break;
        }
    }
    return stream.str();
}

enum class Reg : std::uint8_t {
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,
};

enum class Family : std::uint8_t {
    BinaryRegReg,
    BinaryRegImm,
    UnaryReg,
    ShiftImm,
    ShiftCl,
    BitOps,
    FlagOps,
    MoveOps,
    MemoryOps,
    StackOps,
    StringOps,
    VectorOps,
    ControlFlow,
};

enum class BinaryOp : std::uint8_t {
    Add,
    Adc,
    Sub,
    Sbb,
    And,
    Or,
    Xor,
    Cmp,
    Test,
};

enum class UnaryOp : std::uint8_t {
    Inc,
    Dec,
    Neg,
    Not,
    Mul,
    Imul,
    Div,
    Idiv,
};

enum class ShiftOp : std::uint8_t {
    Rol,
    Ror,
    Rcl,
    Rcr,
    Shl,
    Shr,
    Sar,
};

enum class BitOp : std::uint8_t {
    BtImm,
    BtReg,
    BtMemImm,
    BtsImm,
    BtsReg,
    BtsMemImm,
    BtrImm,
    BtrReg,
    BtrMemImm,
    BtcImm,
    BtcReg,
    BtcMemImm,
    Bsf,
    Bsr,
    Popcnt,
    Bswap,
    BsfAlt,
    BswapAlt,
    Tzcnt,
    Lzcnt,
    ImulRegReg,
    ImulRegImm8,
    ImulRegImm32,
    Crc32Byte,
    Crc32Word,
    Crc32WordF2Then66,
    Crc32Dword,
    Crc32Qword,
    Crc32MemByte,
    Crc32MemWord,
    Crc32MemDword,
    Crc32MemQword,
};

enum class FlagProgram : std::uint8_t {
    Setcc,
    Cmovcc,
    Jcc,
    Cmc,
    Clc,
    Stc,
    Cld,
    Lahf,
    Sahf,
};

enum class MoveProgram : std::uint8_t {
    MovA,
    MovB,
    MovzxByte,
    MovzxWord,
    MovsxByte,
    MovsxWord,
    Movsxd,
    Lea2,
    Lea4,
    Cbw,
    Cwde,
    Cdqe,
    Cwd,
    Cdq,
    Cqo,
    XchgRegReg,
    XchgRaxR8,
    MovbeLoad16,
    MovbeLoad32,
    MovbeLoad64,
    MovbeStore16,
    MovbeStore32,
    MovbeStore64,
};

enum class MemoryProgram : std::uint8_t {
    MovRoundtrip,
    AddMem,
    SubMem,
    AndMem,
    OrMem,
    XorMem,
    XaddMem,
    CmpxchgMem,
    XaddReg,
    CmpxchgRegEqual,
    CmpxchgRegNotEqual,
};

enum class StackProgram : std::uint8_t {
    PushPopPair,
    PushPopFlags,
    EnterLeave,
    PushChain,
    PushImm8Pop,
    PushImm32Pop,
};

enum class StringProgram : std::uint8_t {
    Movsb,
    Movsw,
    Movsd,
    Movsq,
    RepMovsb,
    RepMovsw,
    Lodsb,
    Lodsq,
    Stosb,
    Stosq,
    Cmpsb,
    Scasb,
    Xlat,
};

enum class VectorProgram : std::uint8_t {
    Paddq,
    Pxor,
    Pand,
    Por,
    Sha256Rnds2,
    Sha256Msg1,
    Sha256Msg2,
    Pblendw,
    Vpblendw128,
    Vpblendw256,
    Vinsertf128,
    MovdMmReg,
    MovqMmReg,
    MovdMmMem,
    MovqMmMem,
    MovdXmmReg,
    MovqXmmReg,
    MovdXmmMem,
    MovqXmmMem,
    Punpcklbw,
    Punpcklwd,
    Punpckldq,
    Punpcklqdq,
    Pcmpeqb,
    Pcmpeqw,
    Pcmpeqd,
    Pshufd,
    Pslldq,
    Psrldq,
    PaddqPxor,
    Movdqu,
    MovapsMemDisp8Neg,
    Pshufb,
    Aesenc,
    Aesenclast,
    Aesdec,
    Aesdeclast,
    Aeskeygenassist,
    Roundsd,
    Roundss,
    Vfmadd132sd,
    Vfmadd213sd,
    Vfmadd231sd,
    Pcmpestrm,
    Pcmpestri,
    Pcmpistrm,
};

enum class ControlProgram : std::uint8_t {
    JmpSkip,
    CallAdd,
    CallXor,
    CallLea,
    Loopnz,
    Loopz,
    Loop,
    Nop,
    NopModrm,
    Pause,
};

struct PairSpec {
    Reg dst;
    Reg src;
    const char* name;
};

struct RegSpec {
    Reg reg;
    const char* name;
};

struct CondSpec {
    std::uint8_t code;
    const char* name;
};

struct ProgramSpec {
    Family family;
    std::uint32_t op;
    std::uint32_t variant;
    std::uint64_t flag_mask;
    std::string name;
};

struct FlagComparePolicy {
    std::uint64_t defined_output_mask = 0;
    std::uint64_t must_preserve_mask = 0;
    bool compare_all_defined = false;
    const char* reason_if_not_compared = "no architecturally defined RFLAGS outputs are compared for this generated case";
};

struct ComparePolicy {
    bool compare_gprs = true;
    bool compare_rip = false;
    bool compare_rsp_normalized = false;
    bool compare_mxcsr = false;
    bool compare_xmm = false;
    bool compare_ymm = false;
    bool compare_x87 = false;
    const char* rip_reason_if_not_compared = "native final RIP is not captured as a pass/fail signal";
    const char* mxcsr_reason_if_not_compared = "MXCSR is diagnostic-only unless a generated case stores the effect into compared state";
    const char* xmm_reason_if_not_compared = "vector effects are compared through generated data regions unless explicitly enabled";
    const char* ymm_reason_if_not_compared = "YMM upper state is diagnostic-only unless a generated case stores the effect into compared state";
    const char* x87_reason_if_not_compared = "x87 state is covered by manual cases, not generated differential specs";
    std::array<const char*, 1> memory_regions = {{ "main_data" }};
};

struct BuiltCase {
    ProgramSpec spec;
    std::uint64_t seed;
    std::vector<std::uint8_t> image;
    std::uint32_t data_offset;
    cpueaxh_x86_context initial_context;
};

struct Failure {
    std::string case_name;
    std::string detail;
    std::string spec_name;
    std::string image_hex;
    std::array<std::uint64_t, 16> initial_regs{};
    std::array<std::string, 16> initial_xmm_hex{};
    std::string initial_data_hex;
    std::array<std::uint64_t, 16> native_result_regs{};
    std::array<std::uint64_t, 16> emu_result_regs{};
    std::string native_result_data_hex;
    std::string emu_result_data_hex;
    std::uint64_t seed = 0;
    std::uint64_t seed_index = 0;
    std::uint64_t initial_rip = 0;
    std::uint64_t initial_rflags = 0;
    std::uint32_t initial_mxcsr = 0;
    std::uint32_t initial_data_offset = 0;
    std::uint64_t native_result_rip = 0;
    std::uint64_t native_result_rflags = 0;
    std::uint32_t native_result_mxcsr = 0;
    std::uint64_t emu_result_rip = 0;
    std::uint64_t emu_result_rflags = 0;
    std::uint32_t emu_result_mxcsr = 0;
    std::string host_vendor;
    std::string host_brand;
    std::uint32_t host_max_leaf = 0;
    std::uint32_t host_max_leaf7 = 0;
    std::uint32_t host_max_extended_leaf = 0;
    std::uint32_t host_family = 0;
    std::uint32_t host_model = 0;
    std::uint32_t host_stepping = 0;
    bool host_feature_avx = false;
    bool host_feature_avx2 = false;
    bool host_feature_fma = false;
    bool host_feature_sha = false;
    bool host_feature_popcnt = false;
    bool host_feature_ssse3 = false;
    bool host_feature_sse41 = false;
    bool host_feature_sse42 = false;
    bool host_feature_aes = false;
    bool host_feature_rdpid = false;
    bool host_feature_bmi1 = false;
    bool host_feature_lzcnt = false;
    bool host_feature_movbe = false;
    bool has_seed = false;
    bool has_seed_index = false;
    bool has_initial_state = false;
    bool has_result_state = false;
    bool has_host_features = false;
};

struct TestOptions {
    bool list_only = false;
    bool list_manual_only = false;
    bool list_gates_only = false;
    bool run_manual = true;
    bool has_seed_index = false;
    std::uint64_t seed_index = 0;
    std::uint64_t generated_seed_count = kSeedCount;
    bool has_generated_seed_count = false;
    std::string filter;
    std::string exact_case;
    std::string manual_case;
    std::string failure_record_path;
    std::string failures_record_path;
    std::string feature_record_path;
    std::string spec_manifest_path;
    std::string record_bundle_dir;
    std::string replay_path;
    std::vector<std::string> required_features;
    std::vector<std::string> required_specs;
    std::vector<std::string> required_families;
    bool dump_features_only = false;
    bool dump_specs_only = false;
    bool run_regression_corpus = true;
    bool fail_fast = false;
};

struct ManualCaseIndexEntry {
    const char* name;
    const char* category;
    const char* coverage;
};

struct Stage3GateIndexEntry {
    const char* name;
    const char* category;
    const char* coverage;
    const char* command_hint;
};

inline constexpr std::array<ManualCaseIndexEntry, 10> kManualCaseIndex = {{
    { "compat32_control_transfer", "manual", "near/far ret and cross-mode control-transfer edge cases" },
    { "exception_priority", "manual", "memory, stack, canonical-address, and UD exception ordering" },
    { "invalid_prefix_ud", "manual", "LOCK/VEX/EVEX invalid encoding cases" },
    { "x87_state", "manual", "x87 control/status/env and wait/no-wait behavior" },
    { "host_stack_roundtrip", "manual", "host-mode stack and return-slot behavior" },
    { "cached_rmw_recompute", "manual", "cached decoded RMW recomputation for xchg/xadd/cmpxchg" },
    { "context_api", "manual", "public context read/write register-file coupling" },
    { "simd_encoding_edges", "manual", "selected SSE/AVX/EVEX prefix and register-extension edge cases" },
    { "port_io_escape", "unsafe-native", "I/O instructions are escape/model tested rather than native user-mode executed" },
    { "privileged_system", "unsafe-native", "privileged/MSR/VMX/SVM/system semantics require model or controlled-runner tests" },
}};

inline void print_manual_case_index() {
    std::cout << "cpueaxh manual/unsafe-native test index: " << kManualCaseIndex.size() << std::endl;
    for (const ManualCaseIndexEntry& entry : kManualCaseIndex) {
        std::cout << entry.name << " [" << entry.category << "] " << entry.coverage << std::endl;
    }
}

inline const ManualCaseIndexEntry* find_manual_case_index_entry(const std::string& name) {
    for (const ManualCaseIndexEntry& entry : kManualCaseIndex) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

inline constexpr std::array<Stage3GateIndexEntry, 7> kStage3GateIndex = {{
    { "public_helper_full_regression", "required", "decoder/executor/dispatch/memory/flags helper changes must run the full regression suite", "test.exe --record-bundle failure-bundle" },
    { "generated_long_fuzz", "extended", "broader generated differential fuzz for changed instruction families", "test.exe --generated-seeds 512 --record-bundle failure-bundle" },
    { "undefined_flags", "targeted", "defined RFLAGS masks must remain explicit; undefined flags must not be compared", "test.exe --filter _rr_ --generated-seeds 256 --record-bundle failure-bundle" },
    { "exception_priority", "manual", "manual exception ordering and split-access cases guard decoder/executor rollback behavior", "test.exe --list-manual" },
    { "memory_access_order", "targeted", "memory/RMW/string cases guard guest memory ordering and writeback semantics", "test.exe --filter mem --generated-seeds 256 --record-bundle failure-bundle" },
    { "simd_state_boundary", "feature-gated", "SIMD tests currently compare data-area effects and selected public register-state paths", "test.exe --filter xmm --generated-seeds 256 --record-bundle failure-bundle" },
    { "hardware_feature_matrix", "evidence", "feature evidence must be preserved with CPUID/OS-enabled matrix records", "test.exe --dump-features cpu-features.json" },
}};

inline void print_stage3_gate_index() {
    std::cout << "cpueaxh stage3 regression gates: " << kStage3GateIndex.size() << std::endl;
    for (const Stage3GateIndexEntry& entry : kStage3GateIndex) {
        std::cout << entry.name << " [" << entry.category << "] " << entry.coverage
                  << " | " << entry.command_hint << std::endl;
    }
}

inline bool selected_by_filter(const std::string& name, const TestOptions& options) {
    if (!options.exact_case.empty()) {
        return name == options.exact_case;
    }
    return options.filter.empty() || name.find(options.filter) != std::string::npos;
}

inline bool selected_by_filter(const ProgramSpec& spec, const TestOptions& options) {
    return selected_by_filter(spec.name, options);
}

inline const char* gpr_name_by_index(std::size_t index) {
    static constexpr const char* names[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    return index < (sizeof(names) / sizeof(names[0])) ? names[index] : "unknown";
}

inline std::string xmm_hex(const cpueaxh_x86_xmm& value) {
    return hex64(value.high) + hex64(value.low).substr(2);
}

inline std::vector<std::uint8_t> generated_case_initial_data(const BuiltCase& built) {
    if (built.data_offset > built.image.size()) {
        return {};
    }
    const std::size_t offset = static_cast<std::size_t>(built.data_offset);
    if (offset + kDataSize > built.image.size()) {
        return {};
    }
    return std::vector<std::uint8_t>(built.image.begin() + offset, built.image.begin() + offset + kDataSize);
}

inline void attach_generated_result_state(
    Failure& failure,
    const std::array<std::uint64_t, 16>& native_regs,
    std::uint64_t native_rip,
    std::uint64_t native_rflags,
    std::uint32_t native_mxcsr,
    const std::vector<std::uint8_t>& native_data,
    const std::array<std::uint64_t, 16>& emu_regs,
    std::uint64_t emu_rip,
    std::uint64_t emu_rflags,
    std::uint32_t emu_mxcsr,
    const std::vector<std::uint8_t>& emu_data) {
    failure.native_result_regs = native_regs;
    failure.native_result_rip = native_rip;
    failure.native_result_rflags = native_rflags;
    failure.native_result_mxcsr = native_mxcsr;
    failure.native_result_data_hex = bytes_hex(native_data);
    failure.emu_result_regs = emu_regs;
    failure.emu_result_rip = emu_rip;
    failure.emu_result_rflags = emu_rflags;
    failure.emu_result_mxcsr = emu_mxcsr;
    failure.emu_result_data_hex = bytes_hex(emu_data);
    failure.has_result_state = true;
}

inline void attach_built_case_to_failure(Failure& failure, const BuiltCase& built, std::uint64_t seed_index) {
    if (failure.case_name.empty()) {
        failure.case_name = built.spec.name + ":" + std::to_string(built.seed);
    }
    failure.spec_name = built.spec.name;
    failure.image_hex = bytes_hex(built.image);
    failure.seed = built.seed;
    failure.seed_index = seed_index;
    for (std::size_t index = 0; index < failure.initial_regs.size(); ++index) {
        failure.initial_regs[index] = built.initial_context.regs[index];
    }
    for (std::size_t index = 0; index < failure.initial_xmm_hex.size(); ++index) {
        failure.initial_xmm_hex[index] = xmm_hex(built.initial_context.xmm[index]);
    }
    const std::vector<std::uint8_t> initial_data = generated_case_initial_data(built);
    failure.initial_data_hex = bytes_hex(initial_data);
    failure.initial_rip = built.initial_context.rip;
    failure.initial_rflags = built.initial_context.rflags;
    failure.initial_mxcsr = built.initial_context.mxcsr;
    failure.initial_data_offset = built.data_offset;
    failure.has_seed = true;
    failure.has_seed_index = true;
    failure.has_initial_state = true;
}

inline bool ensure_parent_directory(const std::string& path) {
    const std::filesystem::path file_path(path);
    const std::filesystem::path parent = file_path.parent_path();
    if (parent.empty()) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        std::cerr << "failed to create directory: " << parent.string() << ": " << ec.message() << std::endl;
        return false;
    }
    return true;
}

inline const char* json_bool(bool value) {
    return value ? "true" : "false";
}

inline bool write_failure_record(const std::string& path, const Failure& failure) {
    if (path.empty()) {
        return true;
    }

    if (!ensure_parent_directory(path)) {
        return false;
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        std::cerr << "failed to open failure record: " << path << std::endl;
        return false;
    }

    file << "{\n";
    file << "  \"schema\": \"cpueaxh.failure.v1\",\n";
    file << "  \"case_name\": \"" << json_escape(failure.case_name) << "\",\n";
    file << "  \"detail\": \"" << json_escape(failure.detail) << "\"";
    if (!failure.spec_name.empty()) {
        file << ",\n  \"spec_name\": \"" << json_escape(failure.spec_name) << "\"";
    }
    if (failure.has_seed_index) {
        file << ",\n  \"seed_index\": " << failure.seed_index;
    }
    if (failure.has_seed) {
        file << ",\n  \"seed\": \"" << failure.seed << "\"";
    }
    if (!failure.image_hex.empty()) {
        file << ",\n  \"image_hex\": \"" << json_escape(failure.image_hex) << "\"";
    }
    if (failure.has_initial_state) {
        file << ",\n  \"initial_state\": {\n";
        file << "    \"schema\": \"cpueaxh.generated-initial-state.v1\",\n";
        file << "    \"gprs\": {";
        for (std::size_t index = 0; index < failure.initial_regs.size(); ++index) {
            if (index != 0) {
                file << ",";
            }
            file << "\n      \"" << gpr_name_by_index(index) << "\": \"" << hex64(failure.initial_regs[index]) << "\"";
        }
        file << "\n    },\n";
        file << "    \"rip\": \"" << hex64(failure.initial_rip) << "\",\n";
        file << "    \"rflags\": \"" << hex64(failure.initial_rflags) << "\",\n";
        file << "    \"mxcsr\": \"" << hex64(static_cast<std::uint64_t>(failure.initial_mxcsr)) << "\",\n";
        file << "    \"xmm\": [";
        for (std::size_t index = 0; index < failure.initial_xmm_hex.size(); ++index) {
            if (index != 0) {
                file << ",";
            }
            file << "\n      \"" << json_escape(failure.initial_xmm_hex[index]) << "\"";
        }
        file << "\n    ],\n";
        file << "    \"data_offset\": " << failure.initial_data_offset << ",\n";
        file << "    \"data_hex\": \"" << json_escape(failure.initial_data_hex) << "\"\n";
        file << "  }";
    }
    if (failure.has_result_state) {
        file << ",\n  \"result_state\": {\n";
        file << "    \"schema\": \"cpueaxh.generated-result-state.v1\",\n";
        file << "    \"native_result\": {\n";
        file << "      \"gprs\": {";
        for (std::size_t index = 0; index < failure.native_result_regs.size(); ++index) {
            if (index != 0) {
                file << ",";
            }
            file << "\n        \"" << gpr_name_by_index(index) << "\": \"" << hex64(failure.native_result_regs[index]) << "\"";
        }
        file << "\n      },\n";
        file << "      \"rip\": \"" << hex64(failure.native_result_rip) << "\",\n";
        file << "      \"rflags\": \"" << hex64(failure.native_result_rflags) << "\",\n";
        file << "      \"mxcsr\": \"" << hex64(static_cast<std::uint64_t>(failure.native_result_mxcsr)) << "\",\n";
        file << "      \"data_hex\": \"" << json_escape(failure.native_result_data_hex) << "\"\n";
        file << "    },\n";
        file << "    \"emu_result\": {\n";
        file << "      \"gprs\": {";
        for (std::size_t index = 0; index < failure.emu_result_regs.size(); ++index) {
            if (index != 0) {
                file << ",";
            }
            file << "\n        \"" << gpr_name_by_index(index) << "\": \"" << hex64(failure.emu_result_regs[index]) << "\"";
        }
        file << "\n      },\n";
        file << "      \"rip\": \"" << hex64(failure.emu_result_rip) << "\",\n";
        file << "      \"rflags\": \"" << hex64(failure.emu_result_rflags) << "\",\n";
        file << "      \"mxcsr\": \"" << hex64(static_cast<std::uint64_t>(failure.emu_result_mxcsr)) << "\",\n";
        file << "      \"data_hex\": \"" << json_escape(failure.emu_result_data_hex) << "\"\n";
        file << "    }\n";
        file << "  }";
    }
    if (failure.has_host_features) {
        file << ",\n  \"host_features\": {\n";
        file << "    \"schema\": \"cpueaxh.host-features.v1\",\n";
        file << "    \"vendor\": \"" << json_escape(failure.host_vendor) << "\",\n";
        file << "    \"brand\": \"" << json_escape(failure.host_brand) << "\",\n";
        file << "    \"max_leaf\": " << failure.host_max_leaf << ",\n";
        file << "    \"max_leaf7\": " << failure.host_max_leaf7 << ",\n";
        file << "    \"max_extended_leaf\": " << failure.host_max_extended_leaf << ",\n";
        file << "    \"family\": " << failure.host_family << ",\n";
        file << "    \"model\": " << failure.host_model << ",\n";
        file << "    \"stepping\": " << failure.host_stepping << ",\n";
        file << "    \"features\": {\n";
        file << "      \"avx\": " << json_bool(failure.host_feature_avx) << ",\n";
        file << "      \"avx2\": " << json_bool(failure.host_feature_avx2) << ",\n";
        file << "      \"fma\": " << json_bool(failure.host_feature_fma) << ",\n";
        file << "      \"sha\": " << json_bool(failure.host_feature_sha) << ",\n";
        file << "      \"popcnt\": " << json_bool(failure.host_feature_popcnt) << ",\n";
        file << "      \"ssse3\": " << json_bool(failure.host_feature_ssse3) << ",\n";
        file << "      \"sse41\": " << json_bool(failure.host_feature_sse41) << ",\n";
        file << "      \"sse42\": " << json_bool(failure.host_feature_sse42) << ",\n";
        file << "      \"aes\": " << json_bool(failure.host_feature_aes) << ",\n";
        file << "      \"rdpid\": " << json_bool(failure.host_feature_rdpid) << ",\n";
        file << "      \"bmi1\": " << json_bool(failure.host_feature_bmi1) << ",\n";
        file << "      \"lzcnt\": " << json_bool(failure.host_feature_lzcnt) << ",\n";
        file << "      \"movbe\": " << json_bool(failure.host_feature_movbe) << "\n";
        file << "    }\n";
        file << "  }";
    }
    if (!failure.spec_name.empty() && failure.has_seed_index) {
        file << ",\n  \"case_selector\": \"" << json_escape(failure.spec_name) << "\"";
        file << ",\n  \"replay_hint\": \"test.exe --case " << json_escape(failure.spec_name)
             << " --seed-index " << failure.seed_index << "\"";
    }
    file << "\n}\n";
    return true;
}

inline bool write_failures_record(const std::string& path, const std::vector<Failure>& failures) {
    if (path.empty()) {
        return true;
    }
    if (!ensure_parent_directory(path)) {
        return false;
    }
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        std::cerr << "failed to open failures record: " << path << std::endl;
        return false;
    }
    file << "{\n";
    file << "  \"schema\": \"cpueaxh.failures.v1\",\n";
    file << "  \"failure_count\": " << failures.size() << ",\n";
    file << "  \"failures\": [\n";
    for (std::size_t index = 0; index < failures.size(); ++index) {
        const Failure& failure = failures[index];
        file << "    { \"case_name\": \"" << json_escape(failure.case_name) << "\", "
             << "\"detail\": \"" << json_escape(failure.detail) << "\"";
        if (!failure.spec_name.empty()) {
            file << ", \"spec_name\": \"" << json_escape(failure.spec_name) << "\"";
        }
        if (failure.has_seed_index) {
            file << ", \"seed_index\": " << failure.seed_index;
        }
        if (failure.has_seed) {
            file << ", \"seed\": \"" << failure.seed << "\"";
        }
        file << " }";
        if (index + 1 != failures.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
    return true;
}

inline bool read_text_file(const std::string& path, std::string& contents) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    contents = stream.str();
    return true;
}

inline void json_skip_ws(const std::string& json, std::size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
        ++pos;
    }
}

inline bool json_parse_string_token(const std::string& json, std::size_t& pos, std::string& value, std::string& error) {
    if (pos >= json.size() || json[pos] != '"') {
        error = "expected JSON string";
        return false;
    }
    ++pos;
    value.clear();
    while (pos < json.size()) {
        const char ch = json[pos++];
        if (ch == '"') {
            return true;
        }
        if (ch == '\\') {
            if (pos >= json.size()) {
                error = "unterminated JSON escape";
                return false;
            }
            const char esc = json[pos++];
            switch (esc) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default:
                error = "unsupported JSON escape";
                return false;
            }
        }
        else {
            if (static_cast<unsigned char>(ch) < 0x20u) {
                error = "control character in JSON string";
                return false;
            }
            value.push_back(ch);
        }
    }
    error = "unterminated JSON string";
    return false;
}

inline bool json_parse_value_syntax_strict(const std::string& json, std::size_t& pos, std::string& error);

inline bool json_parse_array_syntax_strict(const std::string& json, std::size_t& pos, std::string& error) {
    if (pos >= json.size() || json[pos++] != '[') {
        error = "expected JSON array";
        return false;
    }
    json_skip_ws(json, pos);
    if (pos < json.size() && json[pos] == ']') {
        ++pos;
        return true;
    }
    while (pos < json.size()) {
        if (!json_parse_value_syntax_strict(json, pos, error)) {
            return false;
        }
        json_skip_ws(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            json_skip_ws(json, pos);
            if (pos < json.size() && json[pos] == ']') {
                error = "trailing comma in JSON array";
                return false;
            }
            continue;
        }
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return true;
        }
        error = "expected ',' or ']' in JSON array";
        return false;
    }
    error = "unterminated JSON array";
    return false;
}

inline bool json_parse_object_syntax_strict(const std::string& json, std::size_t& pos, std::string& error) {
    if (pos >= json.size() || json[pos++] != '{') {
        error = "expected JSON object";
        return false;
    }
    std::map<std::string, bool> seen;
    json_skip_ws(json, pos);
    if (pos < json.size() && json[pos] == '}') {
        ++pos;
        return true;
    }
    while (pos < json.size()) {
        std::string key;
        if (!json_parse_string_token(json, pos, key, error)) {
            return false;
        }
        if (seen[key]) {
            error = "duplicate nested JSON field: " + key;
            return false;
        }
        seen[key] = true;
        json_skip_ws(json, pos);
        if (pos >= json.size() || json[pos++] != ':') {
            error = "expected ':' after nested JSON key: " + key;
            return false;
        }
        if (!json_parse_value_syntax_strict(json, pos, error)) {
            return false;
        }
        json_skip_ws(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
            json_skip_ws(json, pos);
            if (pos < json.size() && json[pos] == '}') {
                error = "trailing comma in JSON object";
                return false;
            }
            continue;
        }
        if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return true;
        }
        error = "expected ',' or '}' in JSON object";
        return false;
    }
    error = "unterminated JSON object";
    return false;
}

inline bool json_parse_value_syntax_strict(const std::string& json, std::size_t& pos, std::string& error) {
    json_skip_ws(json, pos);
    if (pos >= json.size()) {
        error = "missing JSON value";
        return false;
    }
    if (json[pos] == '"') {
        std::string ignored;
        return json_parse_string_token(json, pos, ignored, error);
    }
    if (json[pos] == '{') {
        return json_parse_object_syntax_strict(json, pos, error);
    }
    if (json[pos] == '[') {
        return json_parse_array_syntax_strict(json, pos, error);
    }
    const std::size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']' &&
           json[pos] != ' ' && json[pos] != '\n' && json[pos] != '\r' && json[pos] != '\t') {
        ++pos;
    }
    if (start == pos) {
        error = "invalid JSON value";
        return false;
    }
    const std::string token = json.substr(start, pos - start);
    if (token == "true" || token == "false" || token == "null") {
        return true;
    }
    const bool numeric = !token.empty() && (token[0] == '-' || (token[0] >= '0' && token[0] <= '9'));
    if (!numeric) {
        error = "invalid JSON scalar";
        return false;
    }
    return true;
}

inline bool json_skip_value_strict(const std::string& json, std::size_t& pos, std::string& error) {
    return json_parse_value_syntax_strict(json, pos, error);
}

struct StrictJsonObject {
    std::map<std::string, std::string> string_values;
    std::map<std::string, std::uint64_t> u64_values;

    bool required_string(const std::string& key, std::string& value) const {
        const auto iter = string_values.find(key);
        if (iter == string_values.end() || iter->second.empty()) {
            return false;
        }
        value = iter->second;
        return true;
    }

    bool required_u64(const std::string& key, std::uint64_t& value) const {
        const auto iter = u64_values.find(key);
        if (iter == u64_values.end()) {
            return false;
        }
        value = iter->second;
        return true;
    }
};

inline bool json_parse_u64_token(const std::string& token, std::uint64_t& value) {
    if (token.empty()) {
        return false;
    }
    std::uint64_t result = 0;
    for (char ch : token) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    value = result;
    return true;
}

inline bool json_parse_top_level_object(
    const std::string& json,
    const std::vector<std::string>& required_keys,
    const std::vector<std::string>& optional_keys,
    bool allow_unknown_keys,
    StrictJsonObject& object,
    std::string& error) {
    std::size_t pos = 0;
    object = StrictJsonObject{};
    json_skip_ws(json, pos);
    if (pos >= json.size() || json[pos++] != '{') {
        error = "JSON document must be a top-level object";
        return false;
    }
    std::map<std::string, bool> allowed;
    std::map<std::string, bool> seen;
    for (const std::string& key : required_keys) {
        allowed[key] = true;
    }
    for (const std::string& key : optional_keys) {
        allowed[key] = true;
    }
    json_skip_ws(json, pos);
    if (pos < json.size() && json[pos] == '}') {
        ++pos;
    }
    else {
        while (pos < json.size()) {
            std::string key;
            if (!json_parse_string_token(json, pos, key, error)) {
                return false;
            }
            if (!allow_unknown_keys && allowed.find(key) == allowed.end()) {
                error = "unknown top-level JSON field: " + key;
                return false;
            }
            if (seen[key]) {
                error = "duplicate top-level JSON field: " + key;
                return false;
            }
            seen[key] = true;
            json_skip_ws(json, pos);
            if (pos >= json.size() || json[pos++] != ':') {
                error = "expected ':' after JSON key: " + key;
                return false;
            }
            json_skip_ws(json, pos);
            if (pos < json.size() && json[pos] == '"') {
                std::string value;
                if (!json_parse_string_token(json, pos, value, error)) {
                    return false;
                }
                object.string_values[key] = value;
            }
            else {
                const std::size_t value_start = pos;
                if (!json_skip_value_strict(json, pos, error)) {
                    return false;
                }
                const std::string token = json.substr(value_start, pos - value_start);
                std::uint64_t u64 = 0;
                if (json_parse_u64_token(token, u64)) {
                    object.u64_values[key] = u64;
                }
            }
            json_skip_ws(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                ++pos;
                json_skip_ws(json, pos);
                if (pos < json.size() && json[pos] == '}') {
                    error = "trailing comma in JSON object";
                    return false;
                }
                continue;
            }
            if (pos < json.size() && json[pos] == '}') {
                ++pos;
                break;
            }
            error = "expected ',' or '}' in JSON object";
            return false;
        }
    }
    json_skip_ws(json, pos);
    if (pos != json.size()) {
        error = "unexpected trailing data after JSON object";
        return false;
    }
    for (const std::string& key : required_keys) {
        if (!seen[key]) {
            error = "missing required top-level JSON field: " + key;
            return false;
        }
    }
    return true;
}

inline bool json_validate_top_level_object(
    const std::string& json,
    const std::vector<std::string>& required_keys,
    const std::vector<std::string>& optional_keys,
    std::string& error) {
    StrictJsonObject object;
    return json_parse_top_level_object(json, required_keys, optional_keys, false, object, error);
}

inline bool manual_replay_hint_matches_case(const std::string& replay_hint, const std::string& manual_case) {
    const std::string option = "--manual-case";
    const std::size_t option_pos = replay_hint.find(option);
    if (option_pos == std::string::npos) {
        return false;
    }

    std::size_t pos = option_pos + option.size();
    if (pos >= replay_hint.size() || replay_hint[pos] != ' ') {
        return false;
    }
    while (pos < replay_hint.size() && replay_hint[pos] == ' ') {
        ++pos;
    }
    if (pos >= replay_hint.size()) {
        return false;
    }

    const std::size_t end = replay_hint.find(' ', pos);
    const std::string replay_case = replay_hint.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    return replay_case == manual_case;
}

inline bool apply_replay_file(const std::string& path, TestOptions& options, std::string& error) {
    std::string json;
    if (!read_text_file(path, json)) {
        error = "failed to read replay file: " + path;
        return false;
    }

    StrictJsonObject schema_object;
    if (!json_parse_top_level_object(json, { "schema" }, {}, true, schema_object, error)) {
        error = "replay JSON schema validation failed: " + error;
        return false;
    }
    std::string schema;
    if (!schema_object.required_string("schema", schema)) {
        error = "replay file has no non-empty schema: " + path;
        return false;
    }

    if (schema == "cpueaxh.manual-index.v1") {
        StrictJsonObject object;
        if (!json_parse_top_level_object(json,
            { "schema", "case_selector", "category", "coverage", "replay" },
            {}, false, object, error)) {
            error = "manual replay JSON schema validation failed: " + error;
            return false;
        }
        std::string manual_case;
        if (!object.required_string("case_selector", manual_case)) {
            error = "manual replay file has no non-empty case_selector: " + path;
            return false;
        }
        const ManualCaseIndexEntry* entry = find_manual_case_index_entry(manual_case);
        if (!entry) {
            error = "manual replay case is not in the manual index: " + manual_case;
            return false;
        }
        std::string category;
        if (!object.required_string("category", category)) {
            error = "manual replay file has no non-empty category: " + path;
            return false;
        }
        if (category != "manual" && category != "unsafe-native") {
            error = "manual replay file has unsupported category: " + category;
            return false;
        }
        if (category != entry->category) {
            error = "manual replay category does not match manual index: " + manual_case;
            return false;
        }
        std::string coverage;
        if (!object.required_string("coverage", coverage)) {
            error = "manual replay file has no non-empty coverage: " + path;
            return false;
        }
        if (coverage != entry->coverage) {
            error = "manual replay coverage does not match manual index: " + manual_case;
            return false;
        }
        std::string replay_hint;
        if (!object.required_string("replay", replay_hint)) {
            error = "manual replay file has no non-empty replay command: " + path;
            return false;
        }
        if (!manual_replay_hint_matches_case(replay_hint, manual_case)) {
            error = "manual replay command does not match case_selector: " + manual_case;
            return false;
        }
        options.manual_case = manual_case;
        options.filter.clear();
        options.exact_case.clear();
        options.has_seed_index = false;
        options.run_manual = true;
        options.run_regression_corpus = false;
        return true;
    }

    if (schema != "cpueaxh.failure.v1") {
        error = "replay file has unsupported schema: " + path;
        return false;
    }
    StrictJsonObject object;
    if (!json_parse_top_level_object(json,
        { "schema", "case_selector", "seed_index" },
        { "case_name", "detail", "spec_name", "seed", "image_hex", "replay_hint", "initial_state", "result_state", "host_features" },
        false, object, error)) {
        error = "generated replay JSON schema validation failed: " + error;
        return false;
    }

    std::string exact_case;
    if (!object.required_string("case_selector", exact_case)) {
        error = "replay file has no non-empty case_selector: " + path;
        return false;
    }

    std::uint64_t seed_index = 0;
    if (!object.required_u64("seed_index", seed_index)) {
        error = "replay file has no numeric seed_index: " + path;
        return false;
    }

    options.exact_case = exact_case;
    options.filter.clear();
    options.manual_case.clear();
    options.seed_index = seed_index;
    options.has_seed_index = true;
    options.run_manual = false;
    options.run_regression_corpus = false;
    return true;
}

inline bool list_regression_replay_files(std::vector<std::string>& files, std::string& error) {
    files.clear();
    const std::filesystem::path regression_dir = std::filesystem::path("test") / "regression";
    std::error_code ec;
    if (!std::filesystem::exists(regression_dir, ec)) {
        error = "regression corpus directory missing: " + regression_dir.string();
        return false;
    }
    if (!std::filesystem::is_directory(regression_dir, ec)) {
        error = "regression corpus path is not a directory: " + regression_dir.string();
        return false;
    }

    std::filesystem::directory_iterator iterator(regression_dir, ec);
    if (ec) {
        error = "failed to enumerate regression corpus: " + ec.message();
        return false;
    }
    const std::filesystem::directory_iterator end;
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            error = "failed to enumerate regression corpus: " + ec.message();
            return false;
        }
        const std::filesystem::directory_entry& entry = *iterator;
        if (!entry.is_regular_file(ec)) {
            if (ec) {
                error = "failed to inspect regression corpus entry: " + ec.message();
                return false;
            }
            continue;
        }
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path().string());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        error = "regression corpus has no replay json files: " + regression_dir.string();
        return false;
    }
    return true;
}

struct HostFeatures {
    std::string vendor;
    std::string brand;
    std::uint32_t max_leaf = 0;
    std::uint32_t max_leaf7 = 0;
    std::uint32_t max_extended_leaf = 0;
    std::uint32_t family = 0;
    std::uint32_t model = 0;
    std::uint32_t stepping = 0;
    bool avx = false;
    bool avx2 = false;
    bool fma = false;
    bool sha = false;
    bool popcnt = false;
    bool ssse3 = false;
    bool sse41 = false;
    bool sse42 = false;
    bool aes = false;
    bool rdpid = false;
    bool bmi1 = false;
    bool lzcnt = false;
    bool movbe = false;
};

inline constexpr std::array<std::uint8_t, 4> kAesKeygenAssistImmediates = {{ 0x01, 0x1B, 0x36, 0x80 }};
inline constexpr std::array<std::uint8_t, 6> kRoundImmediates = {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x08 }};
inline constexpr std::array<std::uint8_t, 8> kPshufdImmediates = {{ 0x00, 0x1B, 0x4E, 0x93, 0x39, 0xC6, 0xFF, 0x72 }};
inline constexpr std::array<std::uint8_t, 8> kPcmpestriImmediates = {{ 0x00, 0x08, 0x10, 0x18, 0x40, 0x48, 0x70, 0x78 }};
inline constexpr std::array<std::uint8_t, 4> kVinsertf128Immediates = {{ 0x00, 0x01, 0x40, 0x7F }};

inline constexpr std::array<PairSpec, 3> kBinaryPairs = {{
    { Reg::RAX, Reg::RBX, "rax_rbx" },
    { Reg::R8, Reg::R9, "r8_r9" },
    { Reg::R10, Reg::R11, "r10_r11" },
}};

inline constexpr std::array<RegSpec, 3> kCoreRegs = {{
    { Reg::RAX, "rax" },
    { Reg::R8, "r8" },
    { Reg::R10, "r10" },
}};

inline constexpr std::array<RegSpec, 4> kUnaryRegs = {{
    { Reg::RAX, "rax" },
    { Reg::R8, "r8" },
    { Reg::R10, "r10" },
    { Reg::R13, "r13" },
}};

inline constexpr std::array<CondSpec, 16> kConditions = {{
    { 0x0, "o" },
    { 0x1, "no" },
    { 0x2, "b" },
    { 0x3, "ae" },
    { 0x4, "z" },
    { 0x5, "nz" },
    { 0x6, "be" },
    { 0x7, "a" },
    { 0x8, "s" },
    { 0x9, "ns" },
    { 0xA, "p" },
    { 0xB, "np" },
    { 0xC, "l" },
    { 0xD, "ge" },
    { 0xE, "le" },
    { 0xF, "g" },
}};

constexpr std::uint32_t kConditionTrueVariantBit = 0x100u;

inline std::uint32_t condition_variant(std::uint8_t condition_code, bool should_be_true) {
    return static_cast<std::uint32_t>(condition_code) | (should_be_true ? kConditionTrueVariantBit : 0u);
}

inline std::uint8_t condition_variant_code(std::uint32_t variant) {
    return static_cast<std::uint8_t>(variant & 0x0Fu);
}

inline bool condition_variant_expected_true(std::uint32_t variant) {
    return (variant & kConditionTrueVariantBit) != 0;
}

inline std::string condition_case_name(const char* prefix, const CondSpec& condition, bool should_be_true) {
    return std::string(prefix) + condition.name + (should_be_true ? "_true" : "_false");
}

inline std::uint64_t forced_condition_flags(std::uint8_t condition_code, bool should_be_true) {
    std::uint64_t flags = 0x202ull;
    const auto set = [&flags](std::uint64_t bit, bool value) {
        if (value) {
            flags |= bit;
        }
        else {
            flags &= ~bit;
        }
    };

    switch (condition_code & 0x0F) {
    case 0x0: set(kFlagOF, should_be_true); break;                         // O
    case 0x1: set(kFlagOF, !should_be_true); break;                        // NO
    case 0x2: set(kFlagCF, should_be_true); break;                         // B/C
    case 0x3: set(kFlagCF, !should_be_true); break;                        // AE/NC
    case 0x4: set(kFlagZF, should_be_true); break;                         // Z/E
    case 0x5: set(kFlagZF, !should_be_true); break;                        // NZ/NE
    case 0x6:                                                            // BE: CF || ZF
        set(kFlagCF, should_be_true);
        set(kFlagZF, false);
        break;
    case 0x7:                                                            // A: !CF && !ZF
        set(kFlagCF, !should_be_true);
        set(kFlagZF, false);
        break;
    case 0x8: set(kFlagSF, should_be_true); break;                         // S
    case 0x9: set(kFlagSF, !should_be_true); break;                        // NS
    case 0xA: set(kFlagPF, should_be_true); break;                         // P/PE
    case 0xB: set(kFlagPF, !should_be_true); break;                        // NP/PO
    case 0xC:                                                            // L: SF != OF
        set(kFlagSF, should_be_true);
        set(kFlagOF, false);
        break;
    case 0xD:                                                            // GE: SF == OF
        set(kFlagSF, !should_be_true);
        set(kFlagOF, false);
        break;
    case 0xE:                                                            // LE: ZF || SF != OF
        set(kFlagZF, should_be_true);
        set(kFlagSF, false);
        set(kFlagOF, false);
        break;
    case 0xF:                                                            // G: !ZF && SF == OF
        set(kFlagZF, !should_be_true);
        set(kFlagSF, false);
        set(kFlagOF, false);
        break;
    }
    return flags;
}

struct Crc32CaseSpec {
    BitOp op;
    const char* name;
    Reg dst;
    Reg src;
    bool memory_source;
    int source_bits;
    bool rex_w;
    bool f2_before_66;
};

inline constexpr std::array<Crc32CaseSpec, 9> kCrc32Cases = {{
    { BitOp::Crc32Byte, "crc32_r32_r8", Reg::RAX, Reg::RBX, false, 8, false, false },
    { BitOp::Crc32Word, "crc32_r32_r16", Reg::RAX, Reg::RBX, false, 16, false, false },
    { BitOp::Crc32WordF2Then66, "crc32_r32_r16_f2_66", Reg::RAX, Reg::RBX, false, 16, false, true },
    { BitOp::Crc32Dword, "crc32_r32_r32", Reg::R8, Reg::R9, false, 32, false, false },
    { BitOp::Crc32Qword, "crc32_r64_r64", Reg::R10, Reg::R11, false, 64, true, false },
    { BitOp::Crc32MemByte, "crc32_r32_m8", Reg::RAX, Reg::RAX, true, 8, false, false },
    { BitOp::Crc32MemWord, "crc32_r32_m16", Reg::R8, Reg::RAX, true, 16, false, false },
    { BitOp::Crc32MemDword, "crc32_r32_m32", Reg::R10, Reg::RAX, true, 32, false, false },
    { BitOp::Crc32MemQword, "crc32_r64_m64", Reg::R11, Reg::RAX, true, 64, true, false },
}};

inline const Crc32CaseSpec* find_crc32_case(BitOp op) {
    for (const Crc32CaseSpec& spec : kCrc32Cases) {
        if (spec.op == op) {
            return &spec;
        }
    }
    return nullptr;
}

struct MovbeCaseSpec {
    MoveProgram op;
    const char* name;
    Reg reg;
    int operand_bits;
    bool store_to_memory;
};

inline constexpr std::array<MovbeCaseSpec, 6> kMovbeCases = {{
    { MoveProgram::MovbeLoad16, "movbe_r16_m16", Reg::RAX, 16, false },
    { MoveProgram::MovbeLoad32, "movbe_r32_m32", Reg::R8, 32, false },
    { MoveProgram::MovbeLoad64, "movbe_r64_m64", Reg::R10, 64, false },
    { MoveProgram::MovbeStore16, "movbe_m16_r16", Reg::RAX, 16, true },
    { MoveProgram::MovbeStore32, "movbe_m32_r32", Reg::R8, 32, true },
    { MoveProgram::MovbeStore64, "movbe_m64_r64", Reg::R10, 64, true },
}};

inline const MovbeCaseSpec* find_movbe_case(MoveProgram op) {
    for (const MovbeCaseSpec& spec : kMovbeCases) {
        if (spec.op == op) {
            return &spec;
        }
    }
    return nullptr;
}

inline const char* reg_name(Reg reg) {
    static constexpr const char* names[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    return names[static_cast<std::size_t>(reg)];
}

inline const char* binary_name(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add: return "add";
    case BinaryOp::Adc: return "adc";
    case BinaryOp::Sub: return "sub";
    case BinaryOp::Sbb: return "sbb";
    case BinaryOp::And: return "and";
    case BinaryOp::Or: return "or";
    case BinaryOp::Xor: return "xor";
    case BinaryOp::Cmp: return "cmp";
    default: return "test";
    }
}

inline const char* unary_name(UnaryOp op) {
    switch (op) {
    case UnaryOp::Inc: return "inc";
    case UnaryOp::Dec: return "dec";
    case UnaryOp::Neg: return "neg";
    case UnaryOp::Not: return "not";
    case UnaryOp::Mul: return "mul";
    case UnaryOp::Imul: return "imul";
    case UnaryOp::Div: return "div";
    default: return "idiv";
    }
}

inline const char* shift_name(ShiftOp op) {
    switch (op) {
    case ShiftOp::Rol: return "rol";
    case ShiftOp::Ror: return "ror";
    case ShiftOp::Rcl: return "rcl";
    case ShiftOp::Rcr: return "rcr";
    case ShiftOp::Shl: return "shl";
    case ShiftOp::Shr: return "shr";
    default: return "sar";
    }
}

inline std::uint64_t binary_flag_mask(BinaryOp op) {
    switch (op) {
    case BinaryOp::And:
    case BinaryOp::Or:
    case BinaryOp::Xor:
    case BinaryOp::Test:
        return kLogicMask;
    default:
        return kStatusMask;
    }
}

inline std::uint64_t unary_flag_mask(UnaryOp op) {
    switch (op) {
    case UnaryOp::Inc:
    case UnaryOp::Dec:
        return kIncDecMask;
    case UnaryOp::Neg:
        return kStatusMask;
    case UnaryOp::Mul:
    case UnaryOp::Imul:
        return kRotationMask;
    case UnaryOp::Div:
    case UnaryOp::Idiv:
        return 0;
    default:
        return 0;
    }
}

inline std::uint64_t shift_flag_mask(ShiftOp op) {
    switch (op) {
    case ShiftOp::Rol:
    case ShiftOp::Ror:
    case ShiftOp::Rcl:
    case ShiftOp::Rcr:
        return kRotationMask;
    default:
        return kLogicMask;
    }
}

inline std::uint64_t make_initial_flags(std::uint64_t seed) {
    std::uint64_t flags = 0x202ull;
    std::uint64_t bits = seeded(seed, 0x55);
    if (bits & 0x01) flags |= kFlagCF;
    if (bits & 0x02) flags |= kFlagPF;
    if (bits & 0x04) flags |= kFlagAF;
    if (bits & 0x08) flags |= kFlagZF;
    if (bits & 0x10) flags |= kFlagSF;
    if (bits & 0x20) flags |= kFlagOF;
    return flags;
}

inline std::string trim_cpu_brand_string(const char* text) {
    std::string value(text ? text : "");
    while (!value.empty() && (value.back() == ' ' || value.back() == '\0')) {
        value.pop_back();
    }
    std::size_t start = 0;
    while (start < value.size() && value[start] == ' ') {
        ++start;
    }
    return start == 0 ? value : value.substr(start);
}

inline HostFeatures query_host_features() {
    HostFeatures features;
    int cpu_info[4] = {};
    __cpuid(cpu_info, 0);
    const int max_leaf = cpu_info[0];
    features.max_leaf = static_cast<std::uint32_t>(max_leaf);
    char vendor[13] = {};
    std::memcpy(vendor + 0, &cpu_info[1], sizeof(cpu_info[1]));
    std::memcpy(vendor + 4, &cpu_info[3], sizeof(cpu_info[3]));
    std::memcpy(vendor + 8, &cpu_info[2], sizeof(cpu_info[2]));
    features.vendor = vendor;
    if (max_leaf >= 1) {
        __cpuidex(cpu_info, 1, 0);
        const std::uint32_t signature = static_cast<std::uint32_t>(cpu_info[0]);
        const std::uint32_t base_stepping = signature & 0xFu;
        const std::uint32_t base_model = (signature >> 4) & 0xFu;
        const std::uint32_t base_family = (signature >> 8) & 0xFu;
        const std::uint32_t extended_model = (signature >> 16) & 0xFu;
        const std::uint32_t extended_family = (signature >> 20) & 0xFFu;
        features.stepping = base_stepping;
        features.family = base_family == 0xFu ? base_family + extended_family : base_family;
        features.model = (base_family == 0x6u || base_family == 0xFu)
            ? base_model + (extended_model << 4)
            : base_model;
        const bool has_fma = (cpu_info[2] & (1 << 12)) != 0;
        features.popcnt = (cpu_info[2] & (1 << 23)) != 0;
        const bool has_xsave = (cpu_info[2] & (1 << 26)) != 0;
        const bool has_osxsave = (cpu_info[2] & (1 << 27)) != 0;
        const bool has_avx = (cpu_info[2] & (1 << 28)) != 0;
        if (has_xsave && has_osxsave && has_avx) {
            const unsigned __int64 xcr0 = _xgetbv(0);
            features.avx = (xcr0 & 0x6) == 0x6;
        }
        features.fma = features.avx && has_fma;
        features.ssse3 = (cpu_info[2] & (1 << 9)) != 0;
        features.sse41 = (cpu_info[2] & (1 << 19)) != 0;
        features.sse42 = (cpu_info[2] & (1 << 20)) != 0;
        features.movbe = (cpu_info[2] & (1 << 22)) != 0;
        features.aes = (cpu_info[2] & (1 << 25)) != 0;
    }
    if (max_leaf >= 7) {
        __cpuidex(cpu_info, 7, 0);
        features.max_leaf7 = static_cast<std::uint32_t>(cpu_info[0]);
        features.avx2 = features.avx && (cpu_info[1] & (1 << 5)) != 0;
        features.bmi1 = (cpu_info[1] & (1 << 3)) != 0;
        features.sha = (cpu_info[1] & (1 << 29)) != 0;
        features.rdpid = (cpu_info[2] & (1 << 22)) != 0;
    }
    __cpuid(cpu_info, 0x80000000);
    const std::uint32_t max_extended_leaf = static_cast<std::uint32_t>(cpu_info[0]);
    features.max_extended_leaf = max_extended_leaf;
    if (max_extended_leaf >= 0x80000001u) {
        __cpuidex(cpu_info, 0x80000001, 0);
        features.lzcnt = (cpu_info[2] & (1 << 5)) != 0;
    }
    if (max_extended_leaf >= 0x80000004u) {
        char brand[49] = {};
        for (std::uint32_t leaf = 0; leaf < 3; ++leaf) {
            __cpuid(cpu_info, static_cast<int>(0x80000002u + leaf));
            std::memcpy(brand + leaf * 16u, cpu_info, sizeof(cpu_info));
        }
        features.brand = trim_cpu_brand_string(brand);
    }
    return features;
}

inline bool write_host_feature_record(const std::string& path, const HostFeatures& features) {
    if (path.empty()) {
        return true;
    }
    if (!ensure_parent_directory(path)) {
        return false;
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        std::cerr << "failed to open feature record: " << path << std::endl;
        return false;
    }

    file << "{\n";
    file << "  \"schema\": \"cpueaxh.host-features.v1\",\n";
    file << "  \"vendor\": \"" << json_escape(features.vendor) << "\",\n";
    file << "  \"brand\": \"" << json_escape(features.brand) << "\",\n";
    file << "  \"max_leaf\": " << features.max_leaf << ",\n";
    file << "  \"max_leaf7\": " << features.max_leaf7 << ",\n";
    file << "  \"max_extended_leaf\": " << features.max_extended_leaf << ",\n";
    file << "  \"family\": " << features.family << ",\n";
    file << "  \"model\": " << features.model << ",\n";
    file << "  \"stepping\": " << features.stepping << ",\n";
    file << "  \"features\": {\n";
    file << "    \"avx\": " << json_bool(features.avx) << ",\n";
    file << "    \"avx2\": " << json_bool(features.avx2) << ",\n";
    file << "    \"fma\": " << json_bool(features.fma) << ",\n";
    file << "    \"sha\": " << json_bool(features.sha) << ",\n";
    file << "    \"popcnt\": " << json_bool(features.popcnt) << ",\n";
    file << "    \"ssse3\": " << json_bool(features.ssse3) << ",\n";
    file << "    \"sse41\": " << json_bool(features.sse41) << ",\n";
    file << "    \"sse42\": " << json_bool(features.sse42) << ",\n";
    file << "    \"aes\": " << json_bool(features.aes) << ",\n";
    file << "    \"rdpid\": " << json_bool(features.rdpid) << ",\n";
    file << "    \"bmi1\": " << json_bool(features.bmi1) << ",\n";
    file << "    \"lzcnt\": " << json_bool(features.lzcnt) << ",\n";
    file << "    \"movbe\": " << json_bool(features.movbe) << "\n";
    file << "  }\n";
    file << "}\n";
    return true;
}

inline bool dump_host_feature_record(const std::string& path) {
    return write_host_feature_record(path, query_host_features());
}

inline void attach_host_features_to_failure(Failure& failure, const HostFeatures& features) {
    failure.host_vendor = features.vendor;
    failure.host_brand = features.brand;
    failure.host_max_leaf = features.max_leaf;
    failure.host_max_leaf7 = features.max_leaf7;
    failure.host_max_extended_leaf = features.max_extended_leaf;
    failure.host_family = features.family;
    failure.host_model = features.model;
    failure.host_stepping = features.stepping;
    failure.host_feature_avx = features.avx;
    failure.host_feature_avx2 = features.avx2;
    failure.host_feature_fma = features.fma;
    failure.host_feature_sha = features.sha;
    failure.host_feature_popcnt = features.popcnt;
    failure.host_feature_ssse3 = features.ssse3;
    failure.host_feature_sse41 = features.sse41;
    failure.host_feature_sse42 = features.sse42;
    failure.host_feature_aes = features.aes;
    failure.host_feature_rdpid = features.rdpid;
    failure.host_feature_bmi1 = features.bmi1;
    failure.host_feature_lzcnt = features.lzcnt;
    failure.host_feature_movbe = features.movbe;
    failure.has_host_features = true;
}

inline const char* family_name(Family family) {
    switch (family) {
    case Family::BinaryRegReg: return "binary_reg_reg";
    case Family::BinaryRegImm: return "binary_reg_imm";
    case Family::UnaryReg: return "unary_reg";
    case Family::ShiftImm: return "shift_imm";
    case Family::ShiftCl: return "shift_cl";
    case Family::BitOps: return "bit_ops";
    case Family::FlagOps: return "flag_ops";
    case Family::MoveOps: return "move_ops";
    case Family::MemoryOps: return "memory_ops";
    case Family::StackOps: return "stack_ops";
    case Family::StringOps: return "string_ops";
    case Family::VectorOps: return "vector_ops";
    case Family::ControlFlow: return "control_flow";
    default: return "unknown";
    }
}

inline std::string generated_instruction_form(const ProgramSpec& spec) {
    return spec.name;
}

inline std::string generated_operation_name(const ProgramSpec& spec) {
    const std::size_t separator = spec.name.find('_');
    if (separator == std::string::npos || separator == 0) {
        return spec.name;
    }
    return spec.name.substr(0, separator);
}

inline const char* generated_feature_gate(const ProgramSpec& spec) {
    const std::string& name = spec.name;
    if (name.find("crc32") != std::string::npos || name.find("pcmp") != std::string::npos) return "sse42";
    if (name.find("pblendw") != std::string::npos || name.find("roundsd") != std::string::npos || name.find("roundss") != std::string::npos) return "sse41";
    if (name.find("pshufb") != std::string::npos) return "ssse3";
    if (name.find("movbe") != std::string::npos) return "movbe";
    if (name.find("popcnt") != std::string::npos) return "popcnt";
    if (name.find("rdpid") != std::string::npos) return "rdpid";
    if (name.find("lzcnt") != std::string::npos) return "lzcnt";
    if (name.find("tzcnt") != std::string::npos) return "bmi1";
    if (name.find("sha") != std::string::npos) return "sha";
    if (name.find("aes") != std::string::npos) return "aes";
    if (name.find("vfmadd") != std::string::npos) return "fma";
    if (name.find("ymm") != std::string::npos || name.find("avx") != std::string::npos || (!name.empty() && name[0] == 'v')) return "avx";
    return "base";
}

inline const char* generated_native_safety(const ProgramSpec&) {
    return "safe_user_mode";
}

inline FlagComparePolicy generated_flag_policy(const ProgramSpec& spec) {
    FlagComparePolicy policy{};
    policy.defined_output_mask = spec.flag_mask;
    policy.must_preserve_mask = 0;
    policy.compare_all_defined = spec.flag_mask != 0;
    policy.reason_if_not_compared = spec.flag_mask == 0
        ? "this generated case has no defined RFLAGS outputs to compare or validates effects through other compared state"
        : "";
    return policy;
}

inline void write_flag_name_array(std::ostream& file, std::uint64_t mask) {
    struct FlagName { std::uint64_t bit; const char* name; };
    const FlagName flags[] = {
        { kFlagCF, "CF" }, { kFlagPF, "PF" }, { kFlagAF, "AF" }, { kFlagZF, "ZF" },
        { kFlagSF, "SF" }, { kFlagDF, "DF" }, { kFlagOF, "OF" }
    };
    file << "[";
    bool first = true;
    for (const FlagName& flag : flags) {
        if ((mask & flag.bit) == 0) {
            continue;
        }
        if (!first) {
            file << ", ";
        }
        first = false;
        file << "\"" << flag.name << "\"";
    }
    file << "]";
}

inline void write_compare_policy(std::ostream& file, const ProgramSpec& spec) {
    const ComparePolicy compare{};
    const FlagComparePolicy flags = generated_flag_policy(spec);
    file << "\"compare\": { ";
    file << "\"gprs\": " << json_bool(compare.compare_gprs) << ", ";
    file << "\"rip\": { \"enabled\": " << json_bool(compare.compare_rip)
         << ", \"reason_if_disabled\": \"" << json_escape(compare.rip_reason_if_not_compared) << "\" }, ";
    file << "\"rsp_normalized\": " << json_bool(compare.compare_rsp_normalized) << ", ";
    file << "\"mxcsr\": { \"enabled\": " << json_bool(compare.compare_mxcsr)
         << ", \"reason_if_disabled\": \"" << json_escape(compare.mxcsr_reason_if_not_compared) << "\" }, ";
    file << "\"xmm\": { \"enabled\": " << json_bool(compare.compare_xmm)
         << ", \"reason_if_disabled\": \"" << json_escape(compare.xmm_reason_if_not_compared) << "\" }, ";
    file << "\"ymm\": { \"enabled\": " << json_bool(compare.compare_ymm)
         << ", \"reason_if_disabled\": \"" << json_escape(compare.ymm_reason_if_not_compared) << "\" }, ";
    file << "\"x87\": { \"enabled\": " << json_bool(compare.compare_x87)
         << ", \"reason_if_disabled\": \"" << json_escape(compare.x87_reason_if_not_compared) << "\" }, ";
    file << "\"memory_regions\": [\"" << compare.memory_regions[0] << "\"], ";
    file << "\"flags\": { \"defined\": ";
    write_flag_name_array(file, flags.defined_output_mask);
    file << ", \"defined_output_mask\": " << flags.defined_output_mask
         << ", \"must_preserve_mask\": " << flags.must_preserve_mask
         << ", \"compare_all_defined\": " << json_bool(flags.compare_all_defined)
         << ", \"reason_if_not_compared\": \"" << json_escape(flags.reason_if_not_compared) << "\" }";
    file << " }";
}

inline bool write_generated_spec_manifest(const std::string& path, const std::vector<ProgramSpec>& specs, const HostFeatures& features) {
    if (path.empty()) {
        return true;
    }
    if (!ensure_parent_directory(path)) {
        return false;
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        std::cerr << "failed to open generated spec manifest: " << path << std::endl;
        return false;
    }

    file << "{\n";
    file << "  \"schema\": \"cpueaxh.generated-specs.v1\",\n";
    file << "  \"spec_count\": " << specs.size() << ",\n";
    file << "  \"features\": {\n";
    file << "    \"avx\": " << json_bool(features.avx) << ",\n";
    file << "    \"avx2\": " << json_bool(features.avx2) << ",\n";
    file << "    \"fma\": " << json_bool(features.fma) << ",\n";
    file << "    \"sha\": " << json_bool(features.sha) << ",\n";
    file << "    \"popcnt\": " << json_bool(features.popcnt) << ",\n";
    file << "    \"ssse3\": " << json_bool(features.ssse3) << ",\n";
    file << "    \"sse41\": " << json_bool(features.sse41) << ",\n";
    file << "    \"sse42\": " << json_bool(features.sse42) << ",\n";
    file << "    \"aes\": " << json_bool(features.aes) << ",\n";
    file << "    \"rdpid\": " << json_bool(features.rdpid) << ",\n";
    file << "    \"bmi1\": " << json_bool(features.bmi1) << ",\n";
    file << "    \"lzcnt\": " << json_bool(features.lzcnt) << ",\n";
    file << "    \"movbe\": " << json_bool(features.movbe) << "\n";
    file << "  },\n";
    file << "  \"specs\": [\n";
    for (std::size_t index = 0; index < specs.size(); ++index) {
        const ProgramSpec& spec = specs[index];
        file << "    { \"name\": \"" << json_escape(spec.name) << "\", "
             << "\"instruction_form\": \"" << json_escape(generated_instruction_form(spec)) << "\", "
             << "\"family\": \"" << family_name(spec.family) << "\", "
             << "\"operation\": \"" << json_escape(generated_operation_name(spec)) << "\", "
             << "\"encoding\": \"generated_by_case_builder\", "
             << "\"operand_shape\": \"" << json_escape(spec.name) << "\", "
             << "\"feature_gate\": \"" << generated_feature_gate(spec) << "\", "
             << "\"native_safety\": \"" << generated_native_safety(spec) << "\", "
             << "\"op\": " << spec.op << ", "
             << "\"variant\": " << spec.variant << ", "
             << "\"flag_mask\": " << spec.flag_mask << ", ";
        write_compare_policy(file, spec);
        file << " }";
        if (index + 1 != specs.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
    return true;
}

inline std::vector<ProgramSpec> make_specs(const HostFeatures& features);

inline bool dump_generated_spec_manifest(const std::string& path) {
    const HostFeatures features = query_host_features();
    return write_generated_spec_manifest(path, make_specs(features), features);
}

inline cpueaxh_x86_context make_initial_context(std::uint64_t seed) {
    cpueaxh_x86_context context{};
    for (std::size_t index = 0; index < 16; ++index) {
        context.regs[index] = seeded(seed, 0x100 + index);
    }
    context.rflags = make_initial_flags(seed);
    context.mxcsr = static_cast<std::uint32_t>(kInitialMxcsr);
    return context;
}

inline cpueaxh_x86_xmm make_seeded_xmm(std::uint64_t seed, std::uint64_t salt) {
    cpueaxh_x86_xmm value{};
    value.low = seeded(seed, salt);
    value.high = seeded(seed, salt + 1);
    return value;
}

inline cpueaxh_x86_ymm make_seeded_ymm(std::uint64_t seed, std::uint64_t salt) {
    cpueaxh_x86_ymm value{};
    value.lower = make_seeded_xmm(seed, salt);
    value.upper = make_seeded_xmm(seed, salt + 2);
    return value;
}

inline cpueaxh_x86_zmm make_seeded_zmm(std::uint64_t seed, std::uint64_t salt) {
    cpueaxh_x86_zmm value{};
    value.lower = make_seeded_ymm(seed, salt);
    value.upper = make_seeded_ymm(seed, salt + 4);
    return value;
}

inline cpueaxh_x86_context make_extended_context(std::uint64_t seed) {
    cpueaxh_x86_context context = make_initial_context(seed);
    for (std::size_t index = 0; index < 16; ++index) {
        context.xmm[index] = make_seeded_xmm(seed, 0x500 + index * 8);
        context.ymm_upper[index] = make_seeded_xmm(seed, 0x580 + index * 8);
        context.zmm_upper[index] = make_seeded_ymm(seed, 0x600 + index * 8);
        context.control_regs[index] = seeded(seed, 0x900 + index);
    }
    for (std::size_t index = 0; index < 8; ++index) {
        context.opmask[index] = seeded(seed, 0x680 + index);
        context.mm[index] = seeded(seed, 0x700 + index);
    }
    context.mxcsr = static_cast<std::uint32_t>(0x1F80u | (seeded(seed, 0x800) & 0x1F3Fu));
    context.es.selector = static_cast<std::uint16_t>(seeded(seed, 0xA00) & 0xFFF8u);
    context.cs.selector = static_cast<std::uint16_t>(seeded(seed, 0xA01) & 0xFFF8u);
    context.ss.selector = static_cast<std::uint16_t>(seeded(seed, 0xA02) & 0xFFF8u);
    context.ds.selector = static_cast<std::uint16_t>(seeded(seed, 0xA03) & 0xFFF8u);
    context.fs.selector = static_cast<std::uint16_t>(seeded(seed, 0xA04) & 0xFFF8u);
    context.gs.selector = static_cast<std::uint16_t>(seeded(seed, 0xA05) & 0xFFF8u);
    context.es.descriptor.base = seeded(seed, 0xA10);
    context.cs.descriptor.base = seeded(seed, 0xA11);
    context.ss.descriptor.base = seeded(seed, 0xA12);
    context.ds.descriptor.base = seeded(seed, 0xA13);
    context.fs.descriptor.base = seeded(seed, 0xA14);
    context.gs.descriptor.base = seeded(seed, 0xA15);
    context.es.descriptor.limit = narrow32(seeded(seed, 0xA20));
    context.cs.descriptor.limit = narrow32(seeded(seed, 0xA21));
    context.ss.descriptor.limit = narrow32(seeded(seed, 0xA22));
    context.ds.descriptor.limit = narrow32(seeded(seed, 0xA23));
    context.fs.descriptor.limit = narrow32(seeded(seed, 0xA24));
    context.gs.descriptor.limit = narrow32(seeded(seed, 0xA25));
    context.es.descriptor.type = narrow8(seeded(seed, 0xA30)) & 0x1Fu;
    context.cs.descriptor.type = narrow8(seeded(seed, 0xA31)) & 0x1Fu;
    context.ss.descriptor.type = narrow8(seeded(seed, 0xA32)) & 0x1Fu;
    context.ds.descriptor.type = narrow8(seeded(seed, 0xA33)) & 0x1Fu;
    context.fs.descriptor.type = narrow8(seeded(seed, 0xA34)) & 0x1Fu;
    context.gs.descriptor.type = narrow8(seeded(seed, 0xA35)) & 0x1Fu;
    context.es.descriptor.dpl = narrow8(seeded(seed, 0xA40)) & 0x3u;
    context.cs.descriptor.dpl = narrow8(seeded(seed, 0xA41)) & 0x3u;
    context.ss.descriptor.dpl = narrow8(seeded(seed, 0xA42)) & 0x3u;
    context.ds.descriptor.dpl = narrow8(seeded(seed, 0xA43)) & 0x3u;
    context.fs.descriptor.dpl = narrow8(seeded(seed, 0xA44)) & 0x3u;
    context.gs.descriptor.dpl = narrow8(seeded(seed, 0xA45)) & 0x3u;
    context.es.descriptor.present = narrow8(seeded(seed, 0xA50)) & 1u;
    context.cs.descriptor.present = narrow8(seeded(seed, 0xA51)) & 1u;
    context.ss.descriptor.present = narrow8(seeded(seed, 0xA52)) & 1u;
    context.ds.descriptor.present = narrow8(seeded(seed, 0xA53)) & 1u;
    context.fs.descriptor.present = narrow8(seeded(seed, 0xA54)) & 1u;
    context.gs.descriptor.present = narrow8(seeded(seed, 0xA55)) & 1u;
    context.es.descriptor.granularity = narrow8(seeded(seed, 0xA60)) & 1u;
    context.cs.descriptor.granularity = narrow8(seeded(seed, 0xA61)) & 1u;
    context.ss.descriptor.granularity = narrow8(seeded(seed, 0xA62)) & 1u;
    context.ds.descriptor.granularity = narrow8(seeded(seed, 0xA63)) & 1u;
    context.fs.descriptor.granularity = narrow8(seeded(seed, 0xA64)) & 1u;
    context.gs.descriptor.granularity = narrow8(seeded(seed, 0xA65)) & 1u;
    context.es.descriptor.db = narrow8(seeded(seed, 0xA70)) & 1u;
    context.cs.descriptor.db = narrow8(seeded(seed, 0xA71)) & 1u;
    context.ss.descriptor.db = narrow8(seeded(seed, 0xA72)) & 1u;
    context.ds.descriptor.db = narrow8(seeded(seed, 0xA73)) & 1u;
    context.fs.descriptor.db = narrow8(seeded(seed, 0xA74)) & 1u;
    context.gs.descriptor.db = narrow8(seeded(seed, 0xA75)) & 1u;
    context.es.descriptor.long_mode = narrow8(seeded(seed, 0xA80)) & 1u;
    context.cs.descriptor.long_mode = narrow8(seeded(seed, 0xA81)) & 1u;
    context.ss.descriptor.long_mode = narrow8(seeded(seed, 0xA82)) & 1u;
    context.ds.descriptor.long_mode = narrow8(seeded(seed, 0xA83)) & 1u;
    context.fs.descriptor.long_mode = narrow8(seeded(seed, 0xA84)) & 1u;
    context.gs.descriptor.long_mode = narrow8(seeded(seed, 0xA85)) & 1u;
    context.gdtr_base = seeded(seed, 0xB00);
    context.gdtr_limit = static_cast<std::uint16_t>(seeded(seed, 0xB01) & 0xFFFFu);
    context.ldtr_base = seeded(seed, 0xB02);
    context.ldtr_limit = static_cast<std::uint16_t>(seeded(seed, 0xB03) & 0xFFFFu);
    context.cpl = static_cast<std::uint8_t>(seeded(seed, 0xB04) & 0x3u);
    context.code_exception = static_cast<std::uint32_t>(CPUEAXH_EXCEPTION_UD + ((seeded(seed, 0xB05) % 3u)));
    context.error_code_exception = narrow32(seeded(seed, 0xB06));
    context.processor_id = narrow32(seeded(seed, 0xB07));
    return context;
}

inline bool equal_xmm(const cpueaxh_x86_xmm& left, const cpueaxh_x86_xmm& right) {
    return left.low == right.low && left.high == right.high;
}

inline bool equal_ymm(const cpueaxh_x86_ymm& left, const cpueaxh_x86_ymm& right) {
    return equal_xmm(left.lower, right.lower) && equal_xmm(left.upper, right.upper);
}

inline bool equal_zmm(const cpueaxh_x86_zmm& left, const cpueaxh_x86_zmm& right) {
    return equal_ymm(left.lower, right.lower) && equal_ymm(left.upper, right.upper);
}

inline bool equal_segment_descriptor(const cpueaxh_x86_segment_descriptor& left, const cpueaxh_x86_segment_descriptor& right) {
    return left.base == right.base &&
        left.limit == right.limit &&
        left.type == right.type &&
        left.dpl == right.dpl &&
        left.present == right.present &&
        left.granularity == right.granularity &&
        left.db == right.db &&
        left.long_mode == right.long_mode;
}

inline bool equal_segment(const cpueaxh_x86_segment& left, const cpueaxh_x86_segment& right) {
    return left.selector == right.selector && equal_segment_descriptor(left.descriptor, right.descriptor);
}

inline bool equal_context(const cpueaxh_x86_context& left, const cpueaxh_x86_context& right) {
    for (std::size_t index = 0; index < 16; ++index) {
        if (left.regs[index] != right.regs[index]) {
            return false;
        }
        if (!equal_xmm(left.xmm[index], right.xmm[index])) {
            return false;
        }
        if (!equal_xmm(left.ymm_upper[index], right.ymm_upper[index])) {
            return false;
        }
        if (!equal_ymm(left.zmm_upper[index], right.zmm_upper[index])) {
            return false;
        }
        if (left.control_regs[index] != right.control_regs[index]) {
            return false;
        }
    }
    for (std::size_t index = 0; index < 8; ++index) {
        if (left.opmask[index] != right.opmask[index]) {
            return false;
        }
        if (left.mm[index] != right.mm[index]) {
            return false;
        }
    }
    return left.rip == right.rip &&
        left.rflags == right.rflags &&
        left.mxcsr == right.mxcsr &&
        equal_segment(left.es, right.es) &&
        equal_segment(left.cs, right.cs) &&
        equal_segment(left.ss, right.ss) &&
        equal_segment(left.ds, right.ds) &&
        equal_segment(left.fs, right.fs) &&
        equal_segment(left.gs, right.gs) &&
        left.gdtr_base == right.gdtr_base &&
        left.gdtr_limit == right.gdtr_limit &&
        left.ldtr_base == right.ldtr_base &&
        left.ldtr_limit == right.ldtr_limit &&
        left.cpl == right.cpl &&
        left.code_exception == right.code_exception &&
        left.error_code_exception == right.error_code_exception &&
        left.processor_id == right.processor_id;
}

inline std::array<std::uint8_t, kDataSize> make_initial_data(std::uint64_t seed) {
    std::array<std::uint8_t, kDataSize> data{};
    for (std::size_t index = 0; index < data.size(); ++index) {
        data[index] = narrow8(seeded(seed, 0x400 + index));
    }
    for (std::size_t index = 0; index < 32; ++index) {
        data[kSlotSize * 4 + index] = narrow8(index + 1);
        data[kSlotSize * 4 + kBufferSize + index] = narrow8(0x80u + index);
    }
    return data;
}

} // namespace cpueaxh_test

