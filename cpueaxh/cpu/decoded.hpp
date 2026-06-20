#pragma once

// cpu/decoded.hpp - Pre-decoded instruction representation used by the
// instruction cache fast path. The decoder analyses an instruction once and
// stores enough information to bypass prefix scanning, opcode classification
// and the long if/else dispatch ladder on subsequent executions.
//
// We intentionally keep the existing execute_* implementations untouched: the
// only thing the cache stores per instruction is the function pointer that the
// dispatcher would have selected, plus the cached instruction bytes so the
// fetch path can be skipped on cache hits.

#include "../cpueaxh_platform.hpp"
#include "def.h"

struct CPU_CONTEXT;

// Maximum bytes we copy into a decoded entry. Mirrors MAX_INST_FETCH but the
// header that exposes that constant is included after this file, so we keep an
// independent definition here. They must remain in sync.
#define DECODED_INST_MAX_BYTES 15

// Handler signature shared by every legacy execute_* function. They take the
// instruction bytes plus the fetched length and operate directly on the
// CPU context.
using DecodedExecuteFn = void(*)(CPU_CONTEXT*, uint8_t*, size_t);

// Fast handler signature: receives the cached DecodedInst directly, so the
// implementation can run the instruction's semantics with zero per-step
// decoding. Used by execute_*_fast wrappers that share the executor body
// with the legacy decoder-driven path.
struct DecodedInst;
using DecodedFastFn = void(*)(CPU_CONTEXT*, const DecodedInst*);

// Prefix bits the legacy execute_* functions read from CPU_CONTEXT after
// decode_* populates them as a side effect. The fast path stamps the cached
// values back into ctx before invoking the shared executor body so behaviour
// matches the legacy decode-and-execute flow byte for byte.
struct DecodedPrefixState {
    uint8_t rex_present;          // 0/1
    uint8_t rex_w;                // 0/1
    uint8_t rex_r;                // 0/1
    uint8_t rex_x;                // 0/1
    uint8_t rex_b;                // 0/1
    uint8_t operand_size_override; // 0/1
    uint8_t address_size_override; // 0/1
};

enum DecodedFlags : uint16_t {
    // Handler advances ctx->rip itself (Jcc / JMP / CALL / RET / IRET, plus
    // the indirect 0xFF /2..5 and 0xFE forms). When this flag is set the
    // executor must NOT add last_inst_size after running the handler.
    DECODED_FLAG_BRANCH = 1u << 0,

    // Handler may change vector / SIMD state. When set, cpu_step_with_prefetch
    // captures a vector snapshot before invoking the handler so an exception
    // can roll it back. Cleared for plain ALU / control-flow instructions to
    // avoid the ~1KB memcpy on every hot iteration.
    DECODED_FLAG_TOUCHES_VECTOR = 1u << 1,

    // Handler returned control with last_inst_size pre-populated by the cached
    // value (set during decode). Used by RET/RET imm16 inline paths that we
    // want to keep evaluating directly in the executor.
    DECODED_FLAG_INLINE_RET = 1u << 2,

    // Decoder hit something it could not classify (UD or unknown). The
    // executor raises #UD without invoking any handler.
    DECODED_FLAG_UD = 1u << 3,

    // Cached "INT3 default escape" path. The executor raises #BP instead of
    // #UD when this flag is set, matching cpu_step_with_prefetch's INT3 fast
    // exit. Kept separate from DECODED_FLAG_UD so they can be distinguished
    // at execute time without re-decoding.
    DECODED_FLAG_BP = 1u << 4,

    // Decoder has proven that the instruction cannot raise any architectural
    // fault that would require rolling back guest state (no #PF / #GP / #SS /
    // #DE / #UD / #AC). When set, the executor skips the per-step scalar
    // snapshot, which is the dominant cost on tight ALU/control-flow loops.
    // Default (flag clear) preserves the legacy snapshot-and-rollback
    // behaviour, so a missed classification only loses speed, never
    // correctness.
    DECODED_FLAG_NO_FAULT = 1u << 5,

    // Decoder has proven this instruction is NOT one of the "escape" opcodes
    // the host can intercept via cpueaxh_escape_add() (INT3, INT, HLT, IN,
    // OUT, INS, SYSCALL, SYSENTER, CPUID, RDTSC, RDTSCP, XGETBV, RDRAND,
    // READCRX, WRITECRX, RDSSP). Set by cpu_decode_instruction so the
    // hook/escape dispatch loop in cpueaxh_emu_start can skip both the
    // per-step instruction fetch AND the escape classifier whenever a cache
    // hit on this PC reports "definitely not an escape". Default (flag clear)
    // preserves the legacy "always classify on every step" behaviour.
    DECODED_FLAG_KNOWN_NOT_ESCAPE = 1u << 6,

    // REP string instructions may commit completed iterations before a later
    // iteration faults. The exception path must preserve scalar progress
    // (RCX/RSI/RDI/RFLAGS), unlike ordinary single-instruction rollback.
    DECODED_FLAG_PARTIAL_PROGRESS = 1u << 7,
};

// Inline kind discriminator for the small set of instructions the executor
// services without going through a function-pointer call.
enum DecodedInlineKind : uint8_t {
    DECODED_INLINE_NONE = 0,
    DECODED_INLINE_RET_NEAR_NO_IMM, // 0xC3
    DECODED_INLINE_RET_NEAR_IMM16,  // 0xC2 imm16
    // Short Jcc (0x70..0x7F) in 64-bit long mode with no prefix bytes. The
    // executor materialises the branch decision inline using the cached
    // condition code + 8-bit displacement, sidestepping execute_jcc's
    // per-step prefix scan.
    DECODED_INLINE_JCC_SHORT_LONGMODE,
    // Near Jcc (0x0F 0x80..0x8F) in 64-bit long mode with no prefix bytes
    // and a 32-bit signed relative displacement.
    DECODED_INLINE_JCC_NEAR_LONGMODE,
};

struct DecodedInst {
    DecodedExecuteFn handler;     // Legacy handler invoked for this instruction.
    DecodedFastFn    fast_handler; // Optional execute_*_fast(ctx, dec) handler.
    uint16_t flags;               // Bitset of DecodedFlags above.
    uint8_t  length;              // Encoded instruction length in bytes.
    uint8_t  inline_kind;         // DecodedInlineKind for inline executor paths.
    uint8_t  bytes[DECODED_INST_MAX_BYTES]; // Cached instruction bytes.
    uint8_t  byte_count;          // Number of valid bytes in `bytes` (<= 15).
    uint16_t inline_imm16;        // RET imm16 / similar small immediates.
    uint8_t  near_ret_prefix_len; // Prefix length consumed by RET inline path.
    uint8_t  near_ret_op_size;    // Operand size for RET inline path (16/32/64).
    int8_t   segment_override;    // Cached fs:/gs:/cs:/... override (-1 = none).
    int8_t   inline_jcc_disp;     // Short Jcc rel8 displacement (signed).
    uint8_t  inline_jcc_cond;     // Short / near Jcc condition (low 4 bits of opcode).
    int32_t  inline_jcc_disp32;   // Near Jcc rel32 displacement (signed).

    // Cached payload for execute_*_fast handlers. `cached` mirrors the result
    // of running the legacy decode_* function once; `prefix` mirrors the
    // CPU_CONTEXT prefix bits the same decoder writes as a side effect. The
    // executor stamps `prefix` back into ctx before dispatching to the fast
    // handler so the executor body sees the exact same state it would after a
    // fresh decode.
    DecodedInstruction  cached;
    DecodedPrefixState  prefix;
};

inline void decoded_inst_clear(DecodedInst* dec) {
    if (!dec) {
        return;
    }
    CPUEAXH_MEMSET(dec, 0, sizeof(*dec));
}

// Stamp the cached prefix bits back into ctx. Mirrors the side effects every
// legacy decode_* function performs at the top of its body, so the executor
// body sees the same CPU_CONTEXT state it would after a fresh decode.
inline void decoded_inst_apply_prefix(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    ctx->rex_present              = dec->prefix.rex_present              != 0;
    ctx->rex_w                    = dec->prefix.rex_w                    != 0;
    ctx->rex_r                    = dec->prefix.rex_r                    != 0;
    ctx->rex_x                    = dec->prefix.rex_x                    != 0;
    ctx->rex_b                    = dec->prefix.rex_b                    != 0;
    ctx->operand_size_override    = dec->prefix.operand_size_override    != 0;
    ctx->address_size_override    = dec->prefix.address_size_override    != 0;
}

// True when the cached DecodedInstruction encodes a memory operand whose
// effective address depends on live register / RIP state and therefore
// cannot reuse the decoder-time inst.mem_address. The fast handler must
// recompute mem_address via get_effective_address before dispatching to
// the shared executor body in this case.
inline bool decoded_inst_needs_mem_recompute(const DecodedInstruction* cached) {
    return cached->has_modrm && ((cached->modrm >> 6) & 0x03) != 3;
}

// Snapshot of the small CPU_CONTEXT footprint that legacy decode_* helpers
// touch as a side effect. Used by the decoder to run a one-shot decode for
// fast-handler payload extraction without leaking that decode into the live
// guest state. We keep this struct minimal because it sits on the cache-miss
// path and never touches the much larger vector/segment area.
struct DecoderCtxScratch {
    bool rex_present;
    bool rex_w;
    bool rex_r;
    bool rex_x;
    bool rex_b;
    bool operand_size_override;
    bool address_size_override;
    int  segment_override;
    int  last_inst_size;
    CPU_EXCEPTION_STATE exception;
};

inline void decoder_save_ctx_scratch(DecoderCtxScratch* out, const CPU_CONTEXT* ctx) {
    out->rex_present              = ctx->rex_present;
    out->rex_w                    = ctx->rex_w;
    out->rex_r                    = ctx->rex_r;
    out->rex_x                    = ctx->rex_x;
    out->rex_b                    = ctx->rex_b;
    out->operand_size_override    = ctx->operand_size_override;
    out->address_size_override    = ctx->address_size_override;
    out->segment_override         = ctx->segment_override;
    out->last_inst_size           = ctx->last_inst_size;
    out->exception                = ctx->exception;
}

inline void decoder_restore_ctx_scratch(CPU_CONTEXT* ctx, const DecoderCtxScratch* in) {
    ctx->rex_present              = in->rex_present;
    ctx->rex_w                    = in->rex_w;
    ctx->rex_r                    = in->rex_r;
    ctx->rex_x                    = in->rex_x;
    ctx->rex_b                    = in->rex_b;
    ctx->operand_size_override    = in->operand_size_override;
    ctx->address_size_override    = in->address_size_override;
    ctx->segment_override         = in->segment_override;
    ctx->last_inst_size           = in->last_inst_size;
    ctx->exception                = in->exception;
}

inline void decoded_inst_capture_prefix_from_ctx(DecodedInst* dec, const CPU_CONTEXT* ctx) {
    dec->prefix.rex_present              = ctx->rex_present              ? 1 : 0;
    dec->prefix.rex_w                    = ctx->rex_w                    ? 1 : 0;
    dec->prefix.rex_r                    = ctx->rex_r                    ? 1 : 0;
    dec->prefix.rex_x                    = ctx->rex_x                    ? 1 : 0;
    dec->prefix.rex_b                    = ctx->rex_b                    ? 1 : 0;
    dec->prefix.operand_size_override    = ctx->operand_size_override    ? 1 : 0;
    dec->prefix.address_size_override    = ctx->address_size_override    ? 1 : 0;
}
