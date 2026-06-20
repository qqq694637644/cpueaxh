#pragma once

// cpu/decoder.hpp - Translates a fetched instruction byte stream into a
// DecodedInst that records which legacy execute_* handler should service it.
//
// The decoder must only base its decisions on the instruction bytes and on
// the CPU mode key returned by cpu_inst_cache_mode_key(). It must NOT depend
// on transient state such as RFLAGS, GPRs or RSP, because the resulting
// DecodedInst is cached across executions.
//
// On success the populated DecodedInst is enough for the executor to invoke
// the right handler, advance RIP correctly and capture vector snapshots only
// when needed. The decoder reports failure paths (UD, fetch error) by setting
// DECODED_FLAG_UD or by leaving handler == NULL with byte_count == 0.

#pragma once

#include "../cpueaxh_platform.hpp"
#include "def.h"
#include "decoded.hpp"
#include "dispatch_helpers.hpp"
#include "../instructions/all_instructions.hpp"

// Conservative classifier that returns true only when the instruction at
// (buf, fetched) is provably free of any architectural fault that would force
// a guest-state rollback. The decoder tags such entries with
// DECODED_FLAG_NO_FAULT so the executor can skip the per-step scalar
// snapshot (a ~440 byte memcpy). The set of recognised instructions is the
// minimal subset that covers the dominant integer ALU / control-flow shapes
// produced by typical compilers, so a missed classification only loses speed.
inline bool cpu_decoder_classify_no_fault(
    const uint8_t* buf, int fetched, int prefix_len, uint16_t opc,
    uint8_t group_reg, uint8_t group3_reg, uint8_t fe_reg, uint8_t ff_reg,
    uint8_t group2_reg)
{
    if (prefix_len < 0 || prefix_len >= fetched) {
        return false;
    }
    for (int index = 0; index < prefix_len; ++index) {
        if (buf[index] == 0xF0) {
            return false;
        }
        if (buf[index] == 0x66 && (opc == 0x00E9 || (opc >= 0x0F80 && opc <= 0x0F8F))) {
            return false;
        }
    }

    // 1-byte INC/DEC reg encodings (only valid in 32-bit mode; in 64-bit
    // these byte values are REX prefixes and would never reach this branch).
    if (opc >= 0x0040 && opc <= 0x004F) return true;

    // MOV reg, imm8/imm16/imm32/imm64.
    if (opc >= 0x00B0 && opc <= 0x00BF) return true;

    // 0x83 /0..7 imm8 ALU group when ModRM is register form.
    if ((opc == 0x0080 || opc == 0x0081 || opc == 0x0083) && group_reg != 0xFF) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // 0x00..0x05 add, 0x08..0x0D or, ... 0x38..0x3D cmp. The reg-reg form
    // matches column .0/.1 (with mod==3) and the .4/.5 variants are
    // imm-on-AL/EAX which cannot fault.
    const uint8_t raw_opc = (uint8_t)(opc & 0xFF);
    if (raw_opc <= 0x3F && (raw_opc & 0x06) <= 0x05) {
        const uint8_t lo = (uint8_t)(raw_opc & 0x07);
        if (lo == 0x04 || lo == 0x05) return true;
        if ((lo == 0x00 || lo == 0x01 || lo == 0x02 || lo == 0x03)) {
            const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
            if (mod == 3) return true;
        }
    }

    // MOV r/m, r and MOV r, r/m when ModRM is register form.
    if (raw_opc == 0x88 || raw_opc == 0x89 || raw_opc == 0x8A || raw_opc == 0x8B) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // LEA never accesses memory: only the address computation, written into
    // the destination GPR.
    if (raw_opc == 0x8D) return true;

    // TEST/XCHG reg, reg.
    if ((raw_opc == 0x84 || raw_opc == 0x85 || raw_opc == 0x86 || raw_opc == 0x87)) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // MOV r/m, imm (group C6/C7 /0) reg form.
    if ((raw_opc == 0xC6 || raw_opc == 0xC7) && group_reg == 0) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // Shift / rotate group on register operand, including count-by-1 and
    // count-by-CL forms; these never fault.
    if ((raw_opc == 0xC0 || raw_opc == 0xC1 || raw_opc == 0xD0 || raw_opc == 0xD1 ||
         raw_opc == 0xD2 || raw_opc == 0xD3) && group2_reg != 0xFF) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // F6/F7 /0..3 (TEST/NOT/NEG) reg form. /4..5 (MUL/IMUL) cannot fault.
    // /6..7 (DIV/IDIV) can raise #DE so they remain off the safe list.
    if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg != 0xFF) {
        if (group3_reg <= 5) {
            const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
            if (mod == 3) return true;
        }
    }

    // FE /0..1 INC/DEC r/m8 reg form.
    if (raw_opc == 0xFE && fe_reg <= 1) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // FF /0,/1 INC/DEC on register. Control-transfer variants can fault on
    // target validation, and CALL also touches the stack, so they stay
    // snapshot-protected.
    if (raw_opc == 0xFF && (ff_reg == 0 || ff_reg == 1)) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 1);
        if (mod == 3) return true;
    }

    // Two-byte 0F 1E / 0F 1F (NOP-like / endbr) — pure NOP at runtime.
    if (opc == 0x0F1E || opc == 0x0F1F) return true;

    // CMOVcc reg form.
    if (opc >= 0x0F40 && opc <= 0x0F4F) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 2);
        if (mod == 3) return true;
    }

    // SETcc reg form.
    if (opc >= 0x0F90 && opc <= 0x0F9F) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 2);
        if (mod == 3) return true;
    }

    // MOVZX / MOVSX reg, reg form.
    if (opc == 0x0FB6 || opc == 0x0FB7 || opc == 0x0FBE || opc == 0x0FBF) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 2);
        if (mod == 3) return true;
    }

    // BSF / BSR / POPCNT / TZCNT / LZCNT (reg form).
    if (opc == 0x0FBC || opc == 0x0FBD || opc == 0x0FB8) {
        const uint8_t mod = peek_modrm_mod_field(buf, fetched, prefix_len, 2);
        if (mod == 3) return true;
    }

    // BSWAP r32/r64 — no operand byte beyond the opcode.
    if (opc >= 0x0FC8 && opc <= 0x0FCF) return true;

    // Single-byte status-flag manipulators and NOP / PAUSE.
    if (opc == 0x0090 || opc == 0x00F5 || opc == 0x00F8 || opc == 0x00F9 ||
        opc == 0x00FC || opc == 0x00FD) {
        return true;
    }

    return false;
}

// Lightweight helper that mirrors cpu_step_with_prefetch's "RET near" advance.
// Used only by the inline executor paths after a cache hit.
inline void cpu_decoder_set_inline_ret(DecodedInst* dec, uint8_t prefix_len, int operand_size, uint16_t imm16, bool has_imm) {
    dec->inline_kind = has_imm ? DECODED_INLINE_RET_NEAR_IMM16 : DECODED_INLINE_RET_NEAR_NO_IMM;
    dec->near_ret_prefix_len = prefix_len;
    dec->near_ret_op_size = (uint8_t)operand_size;
    dec->inline_imm16 = imm16;
    dec->length = (uint8_t)(prefix_len + (has_imm ? 3 : 1));
    dec->flags |= DECODED_FLAG_BRANCH | DECODED_FLAG_INLINE_RET;
}

// Returns true if `opc` corresponds to one of the escape-owned instructions
// that should default to #UD when no escape callback is registered (matches
// the existing cpu_step_with_prefetch behaviour for INT/IN/OUT/SYSCALL/etc).
inline bool cpu_decoder_is_escape_owned_ud(uint16_t opc) {
    return opc == 0x00CD || opc == 0x0F0B ||
        opc == 0x00E4 || opc == 0x00E5 || opc == 0x00E6 || opc == 0x00E7 ||
        opc == 0x00EC || opc == 0x00ED || opc == 0x00EE || opc == 0x00EF ||
        opc == 0x0F05 || opc == 0x0F34 ||
        opc == 0x00F4 ||           // HLT
        opc == 0x0FA2 ||           // CPUID
        opc == 0x0F20 || opc == 0x0F22; // MOV CRx
}

// Returns true if `opc` could possibly be an escape instruction (intercept
// candidate for cpueaxh_escape_add). The opposite "definitely not an escape"
// is what the hook/escape dispatch loop in cpueaxh_emu_start uses to skip
// the fetch + classifier on cache hits. Mirrors the opcode coverage of
// cpueaxh_classify_escape_instruction, with the conservative addendum that
// 0x0F01 (XGETBV/RDTSCP/SGDT/SIDT/...) and 0x0FC7 (RDRAND/CMPXCHG8B/...) and
// 0x0F1E (RDSSP/NOP-ish CET) are escape-candidates as a whole because their
// sub-encodings determine the actual escape ID.
inline bool cpu_decoder_is_escape_candidate_opc(uint16_t opc) {
    if (opc == 0x00CC || opc == 0x00CD || opc == 0x00F1 || opc == 0x00F4 || opc == 0x0F0B) return true; // INT3 / INT / INT1 / HLT / UD2
    if (opc == 0x00E4 || opc == 0x00E5 || opc == 0x00E6 || opc == 0x00E7 ||
        opc == 0x00EC || opc == 0x00ED || opc == 0x00EE || opc == 0x00EF ||
        opc == 0x006C || opc == 0x006D) {
        return true; // IN / OUT / INS
    }
    if (opc == 0x0F05 || opc == 0x0F34) return true; // SYSCALL / SYSENTER
    if (opc == 0x0FA2 || opc == 0x0F31) return true; // CPUID / RDTSC
    if (opc == 0x0F01) return true;                  // XGETBV / RDTSCP / ...
    if (opc == 0x0FC7) return true;                  // RDRAND (sub-encoded)
    if (opc == 0x0F20 || opc == 0x0F22) return true; // MOV CRx
    if (opc == 0x0F1E) return true;                  // RDSSP / CET NOP
    return false;
}

// Predicate version that checks the special is_xgetbv / is_rdtscp encodings.
// Returns true and fills `*size` for the encoded length when matched.
inline bool cpu_decoder_classify_xgetbv_rdtscp(
    const uint8_t* buf, int fetched, int prefix_len, uint16_t opc,
    bool* out_xgetbv, bool* out_rdtscp)
{
    *out_xgetbv = false;
    *out_rdtscp = false;
    if (opc != 0x0F01 || (prefix_len + 2) >= fetched) {
        return false;
    }
    if (buf[prefix_len + 2] == 0xD0) {
        *out_xgetbv = true;
        return true;
    }
    if (buf[prefix_len + 2] == 0xF9) {
        *out_rdtscp = true;
        return true;
    }
    return false;
}

// Resolve the handler for a two-byte 0x0F xx opcode that previously went
// through try_execute_two_byte_misc_instruction. Returns NULL if the decoder
// has no candidate (caller should mark the instruction as #UD).
inline DecodedExecuteFn cpu_decoder_resolve_two_byte_misc(
    const uint8_t* buf, size_t fetched, int prefix_len,
    uint16_t opc, uint8_t mandatory_prefix, bool* out_ud)
{
    *out_ud = false;
    if (opc >= 0x0F80 && opc <= 0x0F8F) return execute_jcc;
    if (opc >= 0x0F90 && opc <= 0x0F9F) return execute_setcc;
    if (opc == 0x0FA0 || opc == 0x0FA8) return execute_push;
    if (opc == 0x0FA1 || opc == 0x0FA9) return execute_pop;
    if (opc == 0x0F77) return execute_emms;
    if (opc == 0x0F01 && is_serialize_instruction(buf, fetched, prefix_len)) return execute_serialize;
    if (opc == 0x0F0D || opc == 0x0F18 || (opc == 0x0F2B && mandatory_prefix == 0x00)) return execute_sse_misc;
    if ((opc == 0x0F2B && mandatory_prefix == 0x66) ||
        (opc == 0x0FE7 && mandatory_prefix == 0x66) ||
        opc == 0x0FC3) return execute_sse2_store_misc;
    if (opc == 0x0FAE && mandatory_prefix == 0xF3 && (prefix_len + 2) < (int)fetched) {
        const uint8_t modrm = buf[prefix_len + 2];
        if (((modrm >> 6) & 0x03) == 3 && ((modrm >> 3) & 0x07) <= 1) return execute_fsgsbase;
        *out_ud = true;
        return NULL;
    }
    if (opc == 0x0FAE) return execute_sse_state;
    if (opc == 0x0FAF) return execute_imul;
    if (opc == 0x0FA4 || opc == 0x0FA5) return execute_shld;
    if (opc == 0x0FAC || opc == 0x0FAD) return execute_shrd;
    if (opc == 0x0FBE || opc == 0x0FBF) return execute_movsx;
    if (opc == 0x0FB6 || opc == 0x0FB7) return execute_movzx;
    if (opc == 0x0FA3 || opc == 0x0FAB || opc == 0x0FB3 || opc == 0x0FBB || opc == 0x0FBA) return execute_bt;
    if (opc == 0x0FBC || opc == 0x0FBD) return execute_bsf;
    if (opc == 0x0FB8) return execute_popcnt;
    if (opc >= 0x0FC8 && opc <= 0x0FCF) return execute_bswap;
    if (opc == 0x0F31) { *out_ud = true; return NULL; }
    if (opc == 0x0FC0 || opc == 0x0FC1) return execute_xadd;
    if (opc == 0x0FB0 || opc == 0x0FB1) return execute_cmpxchg;
    if (opc == 0x0F33) return execute_rdpmc;
    if (opc == 0x0FC7 && is_rdpid_instruction(buf, fetched, prefix_len)) return execute_rdpid;
    if (opc == 0x0FC7 && is_rdseed_instruction(buf, fetched)) return execute_rdseed;
    if (opc == 0x0FC7 && (prefix_len + 2) < (int)fetched) {
        const uint8_t modrm = buf[prefix_len + 2];
        if ((((modrm >> 3) & 0x07) == 7) && (((modrm >> 6) & 0x03) == 3)) { *out_ud = true; return NULL; }
        if (is_rdrand_instruction(buf, fetched)) { *out_ud = true; return NULL; }
        return execute_cmpxchg8b16b;
    }
    if (opc == 0x0FC7) {
        if (is_rdrand_instruction(buf, fetched)) { *out_ud = true; return NULL; }
        return execute_cmpxchg8b16b;
    }
    return NULL;
}

// Same idea for the dense 0x0F 3A xx group.
inline DecodedExecuteFn cpu_decoder_resolve_0f3a_66(
    const uint8_t* buf, int fetched, int prefix_len, bool* out_ud)
{
    *out_ud = false;
    if (is_pinsr_instruction(buf, fetched, prefix_len)) return execute_pinsr;
    if (is_pextr_instruction(buf, fetched, prefix_len)) return execute_pextr;
    if (is_pblendw_instruction(buf, fetched, prefix_len)) return execute_pblendw;
    if (is_aeskeygenassist_instruction(buf, fetched, prefix_len)) return execute_aeskeygenassist;
    if (is_roundss_instruction(buf, fetched, prefix_len)) return execute_roundss;
    if (is_roundsd_instruction(buf, fetched, prefix_len)) return execute_roundsd;
    if (is_pcmpestrm_instruction(buf, fetched, prefix_len)) return execute_pcmpestrm;
    if (is_pcmpestri_instruction(buf, fetched, prefix_len)) return execute_pcmpestri;
    if (is_pcmpistrm_instruction(buf, fetched, prefix_len)) return execute_pcmpistrm;
    if (is_pcmpistri_instruction(buf, fetched, prefix_len)) return execute_pcmpistri;
    *out_ud = true;
    return NULL;
}

// x87 dispatcher returns its handler. Mirrors try_execute_x87_instruction but
// without invoking the handler so the decoder can cache the choice.
inline DecodedExecuteFn cpu_decoder_resolve_x87(
    const uint8_t* buf, size_t fetched, uint8_t raw_opc, int prefix_len)
{
    if (!buf || fetched == 0) {
        return NULL;
    }

    if (raw_opc == 0xD9 || (raw_opc == 0x9B && (prefix_len + 1) < (int)fetched && buf[prefix_len + 1] == 0xD9)) {
        const uint8_t d9_opcode_offset = (raw_opc == 0x9B) ? 1u : 0u;
        const uint8_t d9_modrm = ((prefix_len + d9_opcode_offset + 1) < (int)fetched) ? buf[prefix_len + d9_opcode_offset + 1] : 0xFF;
        const uint8_t d9_reg = (uint8_t)((d9_modrm >> 3) & 0x07);
        return (d9_reg == 4 || d9_reg == 6) ? execute_x87_env : execute_x87_control;
    }

    if (raw_opc == 0xDB || (raw_opc == 0x9B && (prefix_len + 1) < (int)fetched && buf[prefix_len + 1] == 0xDB)) {
        return execute_x87_init;
    }

    if (raw_opc == 0xDD || raw_opc == 0xDF ||
        (raw_opc == 0x9B && (prefix_len + 1) < (int)fetched &&
            (buf[prefix_len + 1] == 0xDD || buf[prefix_len + 1] == 0xDF))) {
        return execute_x87_status;
    }

    return NULL;
}

// Probe-decode an instruction with a legacy decode_* helper without leaking
// any of its CPU_CONTEXT side effects (prefix bits, last_inst_size, raised
// exceptions). On success returns true and fills `out` plus `out_prefix`
// with the CPU_CONTEXT prefix bits the decoder wrote during the probe and
// the encoded instruction length in `out_length`. On failure (the helper
// raised an exception) returns false and leaves CPU state byte-for-byte
// identical to the pre-probe value.
template <typename DecodeFn>
inline bool cpu_decoder_probe_with_helper(CPU_CONTEXT* ctx, uint8_t* code,
                                          size_t code_size, DecodeFn decoder,
                                          DecodedInstruction* out,
                                          DecodedPrefixState* out_prefix,
                                          uint8_t* out_length)
{
    DecoderCtxScratch saved;
    decoder_save_ctx_scratch(&saved, ctx);
    DecodedInstruction probe = decoder(ctx, code, code_size);
    const bool ok = !cpu_has_exception(ctx);
    if (ok) {
        *out = probe;
        out_prefix->rex_present              = ctx->rex_present              ? 1 : 0;
        out_prefix->rex_w                    = ctx->rex_w                    ? 1 : 0;
        out_prefix->rex_r                    = ctx->rex_r                    ? 1 : 0;
        out_prefix->rex_x                    = ctx->rex_x                    ? 1 : 0;
        out_prefix->rex_b                    = ctx->rex_b                    ? 1 : 0;
        out_prefix->operand_size_override    = ctx->operand_size_override    ? 1 : 0;
        out_prefix->address_size_override    = ctx->address_size_override    ? 1 : 0;
        const int len = ctx->last_inst_size;
        *out_length = (len > 0 && len <= DECODED_INST_MAX_BYTES) ? (uint8_t)len : 0;
    }
    decoder_restore_ctx_scratch(ctx, &saved);
    return ok;
}

// Conservative shape filter: returns true when the cached DecodedInstruction
// is independent of guest register state, meaning we can replay it on every
// cache hit without redecoding. ModRM mod==3 forms are register-only; A0..A3
// and B0..BF use only the immediate; no-ModRM opcodes have no operand byte
// at all.
inline bool cpu_decoder_dec_inst_safe_to_replay(const DecodedInstruction* d) {
    if (!d->has_modrm) {
        return true;
    }
    return ((d->modrm >> 6) & 0x03) == 3;
}

// Variant for instruction families whose execute_*_fast handler is willing
// to recompute the effective address from the cached ModRM/SIB/disp on every
// replay (instead of using the decoder-time inst.mem_address). For those, the
// only field of DecodedInstruction we cannot trust across replays is
// mem_address itself; everything else (opcode, modrm, sib, displacement,
// operand_size, address_size, immediate) is byte-derived and stable. This
// lets memory-form mov / push / pop / etc participate in the cache fast
// path, which was the dominant gap for real-world workloads dominated by
// stack accesses and structure field reads.
inline bool cpu_decoder_dec_inst_safe_to_replay_recompute_addr(const DecodedInstruction* d) {
    // The byte-derived fields are always stable; the decoder doesn't even
    // require the instruction to have a ModRM byte. Memory-only opcodes like
    // moffs (A0/A1/A2/A3) embed their full address in inst.immediate / disp,
    // which is also byte-derived. So this predicate accepts everything that
    // wasn't a probe-time decode failure (probe_length > 0).
    (void)d;
    return true;
}

// Mid-file forward declaration for the per-family attach routine. The actual
// function lives below (after cpu_decode_instruction) so it can reference the
// execute_*_fast wrappers without polluting the dispatch ladder.
inline void cpu_decoder_try_attach_fast_handler(CPU_CONTEXT* ctx,
                                                const uint8_t* buf,
                                                int fetched,
                                                int prefix_len,
                                                uint16_t opc,
                                                uint8_t raw_opc,
                                                DecodedInst* dec);

// Decode a single instruction whose bytes have already been fetched into
// `buf` (length `fetched`). On return `dec` is fully populated. The function
// never modifies CPU state; the executor does that based on `dec`.
//
// Decoder failure modes:
//   - DECODED_FLAG_UD set: handler is NULL, executor must raise #UD.
//   - byte_count == 0: bytes could not be read; executor reports fetch error.
inline void cpu_decode_instruction(
    CPU_CONTEXT* ctx,
    const uint8_t* buf,
    int fetched,
    DecodedInst* dec)
{
    decoded_inst_clear(dec);
    if (!ctx || !buf || fetched <= 0 || !dec) {
        if (dec) {
            dec->byte_count = 0;
        }
        return;
    }

    const int copy_len = fetched > DECODED_INST_MAX_BYTES ? DECODED_INST_MAX_BYTES : fetched;
    CPUEAXH_MEMCPY(dec->bytes, buf, (size_t)copy_len);
    dec->byte_count = (uint8_t)copy_len;
    dec->segment_override = -1;

    // Special multi-byte VEX/EVEX forms intercepted before regular decoding.
    if (buf[0] == 0x62 && is_evex_palignr_instruction(buf, (size_t)fetched)) {
        dec->handler = execute_evex_palignr;
        dec->flags |= DECODED_FLAG_TOUCHES_VECTOR;
        return;
    }
    if (buf[0] == 0x62 && is_evex_pinsrw_instruction(buf, (size_t)fetched)) {
        dec->handler = execute_evex_pinsrw;
        dec->flags |= DECODED_FLAG_TOUCHES_VECTOR;
        return;
    }
    if (buf[0] == 0x62 && is_evex_pinsr_instruction(buf, (size_t)fetched)) {
        dec->handler = execute_evex_pinsr;
        dec->flags |= DECODED_FLAG_TOUCHES_VECTOR;
        return;
    }
    if (buf[0] == 0x62 && is_evex_pextr_instruction(buf, (size_t)fetched)) {
        dec->handler = execute_evex_pextr;
        dec->flags |= DECODED_FLAG_TOUCHES_VECTOR;
        return;
    }
    if (buf[0] == 0xC5 || buf[0] == 0xC4) {
        dec->handler = execute_avx_vex;
        dec->flags |= DECODED_FLAG_TOUCHES_VECTOR;
        return;
    }

    int prefix_len = 0;
    uint16_t opc = peek_opcode(ctx, buf, fetched, &prefix_len);
    if (opc == 0xFFFF) {
        dec->byte_count = 0;
        return;
    }

    dec->segment_override = (int8_t)peek_segment_override(buf, prefix_len);

    bool is_rdtscp = false;
    bool is_xgetbv = false;
    cpu_decoder_classify_xgetbv_rdtscp(buf, fetched, prefix_len, opc, &is_xgetbv, &is_rdtscp);

    const uint8_t raw_opc = (uint8_t)(opc & 0xFF);
    const uint8_t repeat_prefix = peek_repeat_prefix(buf, prefix_len);
    const uint8_t mandatory_prefix = peek_mandatory_prefix(buf, prefix_len);
    const InstructionPrefixPresence prefix_presence = peek_instruction_prefix_presence(buf, prefix_len);

    // Replicates cpu_step_with_prefetch's REX.B detection.
    uint8_t last_rex_prefix = 0;
    for (int i = 0; i < prefix_len; i++) {
        if (cpu_allows_rex_prefix(ctx) && buf[i] >= 0x40 && buf[i] <= 0x4F) {
            last_rex_prefix = buf[i];
        }
    }
    const bool rex_b_prefix = (last_rex_prefix & 0x01) != 0;

    if (cpu_step_may_touch_vector_state(buf, fetched, prefix_len, opc, mandatory_prefix)) {
        dec->flags |= DECODED_FLAG_TOUCHES_VECTOR;
    }

    if (is_branch_opcode(opc)) {
        dec->flags |= DECODED_FLAG_BRANCH;
    }

    // Stamp KNOWN_NOT_ESCAPE before any early-return below so the hook/escape
    // dispatch loop can short-circuit on the very first cache hit, even for
    // UD/BP-classified instructions (those raise their own exception path
    // and are not in the escape-candidate set anyway).
    if (!cpu_decoder_is_escape_candidate_opc(opc)) {
        dec->flags |= DECODED_FLAG_KNOWN_NOT_ESCAPE;
    }

    if (opc == 0x00F4) { dec->flags |= DECODED_FLAG_UD; return; }
    if (opc == 0x00CC) { dec->flags |= DECODED_FLAG_BP; return; }
    if (opc == 0x00F1) { dec->flags |= DECODED_FLAG_DB; return; }
    if (opc == 0x0F0B) { dec->flags |= DECODED_FLAG_UD; return; }
    if (cpu_decoder_is_escape_owned_ud(opc)) {
        dec->flags |= DECODED_FLAG_UD;
        return;
    }

    // RET near 0xC3 / 0xC2 imm16 inline forms (kept in the executor for the
    // tightest possible loop on hot return paths).
    if (opc == 0x00C3) {
        int near_ret_prefix_len = 0;
        const int operand_size = decode_near_ret_operand_size(ctx, buf, fetched, &near_ret_prefix_len);
        cpu_decoder_set_inline_ret(dec, (uint8_t)near_ret_prefix_len, operand_size, 0, false);
        return;
    }
    if (opc == 0x00C2) {
        int near_ret_prefix_len = 0;
        const int operand_size = decode_near_ret_operand_size(ctx, buf, fetched, &near_ret_prefix_len);
        if (fetched < near_ret_prefix_len + 3) {
            // Not enough bytes; signal as UD rather than executing partial.
            dec->flags |= DECODED_FLAG_UD;
            return;
        }
        const uint16_t imm16 = (uint16_t)buf[near_ret_prefix_len + 1] |
            ((uint16_t)buf[near_ret_prefix_len + 2] << 8);
        cpu_decoder_set_inline_ret(dec, (uint8_t)near_ret_prefix_len, operand_size, imm16, true);
        return;
    }

    // Short Jcc rel8 in 64-bit long mode with no prefix bytes. This is the
    // shape produced by every modern compiler for tight conditional loops, so
    // serving it from the cache as an inline path saves a function-pointer
    // call plus execute_jcc's full prefix scan on every iteration.
    //
    // CRITICAL: gate on the full 16-bit `opc` value (0x0070..0x007F), not the
    // raw_opc low byte alone, because two-byte SSE encodings such as 0x0F 0x70
    // (PSHUFW family) collide with the short Jcc opcode space when only the
    // low byte is inspected.
    if (prefix_len == 0 && opc >= 0x0070 && opc <= 0x007F &&
        ctx->cs.descriptor.long_mode && fetched >= 2) {
        dec->inline_kind = (uint8_t)DECODED_INLINE_JCC_SHORT_LONGMODE;
        dec->inline_jcc_cond = (uint8_t)(opc & 0x0F);
        dec->inline_jcc_disp = (int8_t)buf[1];
        dec->length = 2;
        dec->flags |= DECODED_FLAG_BRANCH;
        return;
    }

    // Near Jcc 0x0F 0x80..0x8F in 64-bit long mode without prefix bytes. In
    // long mode operand_size is fixed at 64 and the immediate is always
    // rel32, so the dispatch is a single condition + 32-bit displacement.
    if (prefix_len == 0 && opc >= 0x0F80 && opc <= 0x0F8F &&
        ctx->cs.descriptor.long_mode && fetched >= 6) {
        const int32_t rel32 =
            (int32_t)((uint32_t)buf[2] |
                      ((uint32_t)buf[3] << 8) |
                      ((uint32_t)buf[4] << 16) |
                      ((uint32_t)buf[5] << 24));
        dec->inline_kind = (uint8_t)DECODED_INLINE_JCC_NEAR_LONGMODE;
        dec->inline_jcc_cond = (uint8_t)(opc & 0x0F);
        dec->inline_jcc_disp32 = rel32;
        dec->length = 6;
        dec->flags |= DECODED_FLAG_BRANCH;
        return;
    }

    // Pre-compute group /reg fields once per decode so the lookups below
    // remain O(1).
    uint8_t group_reg = 0xFF;
    uint8_t group3_reg = 0xFF;
    uint8_t fe_reg = 0xFF;
    uint8_t ff_reg = 0xFF;
    uint8_t group2_reg = 0xFF;
    if (raw_opc == 0x80 || raw_opc == 0x81 || raw_opc == 0x83) {
        group_reg = peek_modrm_reg_field(buf, fetched, prefix_len, 1);
    }
    if (raw_opc == 0xF6 || raw_opc == 0xF7) {
        group3_reg = peek_modrm_reg_field(buf, fetched, prefix_len, 1);
    }
    if (raw_opc == 0xFE) {
        fe_reg = peek_modrm_reg_field(buf, fetched, prefix_len, 1);
    }
    if (raw_opc == 0xFF) {
        ff_reg = peek_modrm_reg_field(buf, fetched, prefix_len, 1);
        // Override branch flag: only the indirect call/jmp variants are
        // branches, not INC/DEC/PUSH r/m.
        const bool is_branch_ff = (ff_reg == 2 || ff_reg == 3 || ff_reg == 4 || ff_reg == 5);
        if (is_branch_ff) {
            dec->flags |= DECODED_FLAG_BRANCH;
        }
        else {
            dec->flags = (uint16_t)(dec->flags & ~(uint16_t)DECODED_FLAG_BRANCH);
        }
    }
    if (raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
        raw_opc == 0xC0 || raw_opc == 0xC1) {
        group2_reg = peek_modrm_reg_field(buf, fetched, prefix_len, 1);
    }

    DecodedExecuteFn handler = NULL;

    // The dispatch ladder below mirrors cpu_step_with_prefetch's existing
    // sequence one-for-one. Keeping the ordering intact means the cached
    // decisions match the live dispatcher exactly.

    if (opc == 0x00CB || opc == 0x00CA) {
        handler = execute_ret;
    }
    else if (opc == 0x00CF) {
        handler = execute_iret;
    }
    else if (opc >= 0x0F40 && opc <= 0x0F4F) {
        handler = execute_cmovcc;
    }
    else if ((opc == 0x0F10 || opc == 0x0F11) && mandatory_prefix == 0xF3) {
        handler = execute_movss;
    }
    else if ((opc == 0x0F10 || opc == 0x0F11) && (mandatory_prefix == 0x66 || mandatory_prefix == 0xF2)) {
        handler = execute_sse2_mov_pd;
    }
    else if (opc == 0x0F10 || opc == 0x0F11) {
        handler = execute_movups;
    }
    else if ((opc == 0x0F28 || opc == 0x0F29) && mandatory_prefix == 0x66) {
        handler = execute_sse2_mov_pd;
    }
    else if (opc == 0x0F28 || opc == 0x0F29) {
        handler = execute_movaps;
    }
    else if ((opc == 0x0F60 || opc == 0x0F61 || opc == 0x0F62 || opc == 0x0F63 || opc == 0x0F67 ||
        opc == 0x0F68 || opc == 0x0F69 || opc == 0x0F6A || opc == 0x0F6B || opc == 0x0F6C ||
        opc == 0x0F6D) && mandatory_prefix == 0x66) {
        handler = execute_sse2_pack;
    }
    else if ((opc == 0x0F64 || opc == 0x0F65 || opc == 0x0F66 || opc == 0x0F74 || opc == 0x0F75 ||
        opc == 0x0F76) && mandatory_prefix == 0x66) {
        handler = execute_sse2_int_cmp;
    }
    else if ((opc == 0x0FDB || opc == 0x0FDF || opc == 0x0FEB || opc == 0x0FEF) && mandatory_prefix == 0x66) {
        handler = execute_sse2_int_logic;
    }
    else if (opc == 0x0F38 && is_pshufb_instruction(buf, fetched, prefix_len)) {
        handler = execute_pshufb;
    }
    else if (opc == 0x0F38 && prefix_presence.has_f2 && !prefix_presence.has_f3 && is_crc32_instruction(buf, fetched, prefix_len)) {
        handler = execute_crc32;
    }
    else if (opc == 0x0F38 && !prefix_presence.has_f2 && !prefix_presence.has_f3 && is_movbe_instruction(buf, fetched, prefix_len)) {
        handler = execute_movbe;
    }
    else if (opc == 0x0F38 && mandatory_prefix == 0) {
        if (is_sha256rnds2_instruction(buf, fetched, prefix_len)) handler = execute_sha256rnds2;
        else if (is_sha256msg1_instruction(buf, fetched, prefix_len)) handler = execute_sha256msg1;
        else if (is_sha256msg2_instruction(buf, fetched, prefix_len)) handler = execute_sha256msg2;
        else { dec->flags |= DECODED_FLAG_UD; return; }
    }
    else if (opc == 0x0F38 && mandatory_prefix == 0x66 && is_aesimc_instruction(buf, fetched, prefix_len)) {
        handler = execute_aesimc;
    }
    else if (opc == 0x0F38 && mandatory_prefix == 0x66 && is_aesenc_instruction(buf, fetched, prefix_len)) {
        handler = execute_aesenc;
    }
    else if (opc == 0x0F38 && mandatory_prefix == 0x66 && is_aesenclast_instruction(buf, fetched, prefix_len)) {
        handler = execute_aesenclast;
    }
    else if (opc == 0x0F38 && mandatory_prefix == 0x66 && is_aesdec_instruction(buf, fetched, prefix_len)) {
        handler = execute_aesdec;
    }
    else if (opc == 0x0F38 && mandatory_prefix == 0x66 && is_aesdeclast_instruction(buf, fetched, prefix_len)) {
        handler = execute_aesdeclast;
    }
    else if ((opc == 0x0F71 || opc == 0x0F72 || opc == 0x0F73 ||
        opc == 0x0FD1 || opc == 0x0FD2 || opc == 0x0FD3 ||
        opc == 0x0FE1 || opc == 0x0FE2 ||
        opc == 0x0FF1 || opc == 0x0FF2 || opc == 0x0FF3) && mandatory_prefix == 0x66) {
        handler = execute_sse2_shift;
    }
    else if ((opc == 0x0FD4 || opc == 0x0FD5 || opc == 0x0FD8 || opc == 0x0FD9 || opc == 0x0FDA ||
        opc == 0x0FDC || opc == 0x0FDD || opc == 0x0FDE || opc == 0x0FE0 || opc == 0x0FE3 ||
        opc == 0x0FE4 || opc == 0x0FE5 || opc == 0x0FE8 || opc == 0x0FE9 || opc == 0x0FEA ||
        opc == 0x0FEC || opc == 0x0FED || opc == 0x0FEE || opc == 0x0FF4 || opc == 0x0FF5 ||
        opc == 0x0FF6 || opc == 0x0FF8 || opc == 0x0FF9 || opc == 0x0FFA || opc == 0x0FFB ||
        opc == 0x0FFC || opc == 0x0FFD || opc == 0x0FFE) && mandatory_prefix == 0x66) {
        handler = execute_sse2_int_arith;
    }
    else if (opc == 0x0F6E || opc == 0x0F7E || opc == 0x0FD6) {
        handler = execute_movdq;
    }
    else if ((opc == 0x0F6F || opc == 0x0F7F || opc == 0x0FD7) && (mandatory_prefix == 0x66 || mandatory_prefix == 0xF3)) {
        handler = execute_sse2_mov_int;
    }
    else if (opc == 0x0FF0 || opc == 0x0FF7) {
        handler = execute_sse2_misc;
    }
    else if ((opc == 0x0F12 || opc == 0x0F13 || opc == 0x0F16 || opc == 0x0F17 || opc == 0x0F50) && mandatory_prefix == 0x66) {
        handler = execute_sse2_mov_pd;
    }
    else if (opc == 0x0F12 || opc == 0x0F13 || opc == 0x0F16 || opc == 0x0F17 || opc == 0x0F50) {
        handler = execute_sse_mov_misc;
    }
    else if (((opc == 0x0F14 || opc == 0x0F15 || opc == 0x0FC5 || opc == 0x0FC6) && mandatory_prefix == 0x66) ||
        (opc == 0x0F70 && (mandatory_prefix == 0x66 || mandatory_prefix == 0xF2 || mandatory_prefix == 0xF3))) {
        handler = execute_sse2_shuffle;
    }
    else if (opc == 0x0FC4) {
        handler = execute_pinsrw;
    }
    else if (opc == 0x0F14 || opc == 0x0F15 || opc == 0x0FC6) {
        handler = execute_sse_shuffle;
    }
    else if ((opc == 0x0F54 || opc == 0x0F55 || opc == 0x0F56) && mandatory_prefix == 0x66) {
        handler = execute_sse2_logic_pd;
    }
    else if (opc == 0x0F54 || opc == 0x0F55 || opc == 0x0F56 || opc == 0x0F57) {
        handler = execute_sse_logic;
    }
    else if ((opc == 0x0F58 || opc == 0x0F59 || opc == 0x0F5C || opc == 0x0F5E) && (mandatory_prefix == 0x66 || mandatory_prefix == 0xF2)) {
        handler = execute_sse2_arith_pd;
    }
    else if (opc == 0x0F58 || opc == 0x0F59 || opc == 0x0F5C || opc == 0x0F5E) {
        handler = execute_sse_arith;
    }
    else if ((opc == 0x0F51 || opc == 0x0F5D || opc == 0x0F5F) && (mandatory_prefix == 0x66 || mandatory_prefix == 0xF2)) {
        handler = execute_sse2_math_pd;
    }
    else if (opc == 0x0F51 || opc == 0x0F52 || opc == 0x0F53 || opc == 0x0F5D || opc == 0x0F5F) {
        handler = execute_sse_math;
    }
    else if (opc == 0x0F5A && (mandatory_prefix == 0xF2 || mandatory_prefix == 0xF3)) {
        handler = execute_sse2_scalar_convert;
    }
    else if (opc == 0x0F5A || opc == 0x0F5B || opc == 0x0FE6) {
        handler = execute_sse2_convert;
    }
    else if ((opc == 0x0F2A || opc == 0x0F2C || opc == 0x0F2D) && (mandatory_prefix == 0xF2 || mandatory_prefix == 0x66)) {
        handler = execute_sse2_scalar_convert;
    }
    else if (opc == 0x0F2A || opc == 0x0F2C || opc == 0x0F2D) {
        handler = execute_sse_convert;
    }
    else if ((opc == 0x0FC2 && (mandatory_prefix == 0x66 || mandatory_prefix == 0xF2)) ||
        ((opc == 0x0F2E || opc == 0x0F2F) && mandatory_prefix == 0x66)) {
        handler = execute_sse2_cmp_pd;
    }
    else if (opc == 0x0FC2 || opc == 0x0F2E || opc == 0x0F2F) {
        handler = execute_sse_cmp;
    }
    else {
        bool two_byte_ud = false;
        DecodedExecuteFn two_byte_handler = cpu_decoder_resolve_two_byte_misc(
            buf, (size_t)fetched, prefix_len, opc, mandatory_prefix, &two_byte_ud);
        if (two_byte_handler) {
            handler = two_byte_handler;
        }
        else if (two_byte_ud) {
            dec->flags |= DECODED_FLAG_UD;
            return;
        }
        else if (opc == 0x0F3A && is_palignr_instruction(buf, fetched, prefix_len)) {
            handler = execute_palignr;
        }
        else if (opc == 0x0F3A && mandatory_prefix == 0x66) {
            bool g_ud = false;
            DecodedExecuteFn g_handler = cpu_decoder_resolve_0f3a_66(buf, fetched, prefix_len, &g_ud);
            if (g_handler) {
                handler = g_handler;
            }
            else if (g_ud) {
                dec->flags |= DECODED_FLAG_UD;
                return;
            }
        }
        else if (is_xgetbv) {
            dec->flags |= DECODED_FLAG_UD;
            return;
        }
        else if (is_rdtscp) {
            dec->flags |= DECODED_FLAG_UD;
            return;
        }
        else if (is_endbr_instruction(buf, (size_t)fetched)) {
            handler = execute_endbr;
        }
        else if (is_rdssp_instruction(buf, (size_t)fetched)) {
            handler = execute_rdssp;
        }
        else if (opc == 0x0F1E && (prefix_len + 2) < fetched) {
            const uint8_t opcode3 = buf[prefix_len + 2];
            const uint8_t reg = (uint8_t)((opcode3 >> 3) & 0x07);
            if (opcode3 == 0xFA || opcode3 == 0xFB || reg == 1) {
                dec->flags |= DECODED_FLAG_UD;
                return;
            }
            // 0x0F 0x1E NOP-like form, no handler dispatched in legacy path.
            handler = NULL;
        }
        else if (raw_opc == 0x99) {
            handler = execute_cdq;
        }
        else if (raw_opc == 0x98) {
            handler = execute_cbw;
        }
        else if (raw_opc == 0xC8) {
            handler = execute_enter;
        }
        else if (raw_opc == 0xC9) {
            handler = execute_leave;
        }
        else if (raw_opc == 0x90 && repeat_prefix == 0xF3 && !rex_b_prefix) {
            handler = execute_pause;
        }
        else if (opc == 0x0F1F) {
            handler = execute_nop;
        }
        else if (raw_opc == 0x90 && repeat_prefix == 0x00 && !rex_b_prefix) {
            handler = execute_nop;
        }
        else if ((raw_opc >= 0x90 && raw_opc <= 0x97) || raw_opc == 0x86 || raw_opc == 0x87) {
            handler = execute_xchg;
        }
        else if (raw_opc == 0x63) {
            handler = execute_movsxd;
        }
        else if (raw_opc == 0x8D) {
            handler = execute_lea;
        }
        else if (((raw_opc >= 0x88 && raw_opc <= 0x8C) || raw_opc == 0x8E) ||
            (raw_opc >= 0xA0 && raw_opc <= 0xA3) ||
            (raw_opc >= 0xB0 && raw_opc <= 0xBF) ||
            raw_opc == 0xC6 || raw_opc == 0xC7) {
            handler = execute_mov;
        }
        else if (repeat_prefix != 0 &&
            (raw_opc == 0xA4 || raw_opc == 0xA5 ||
                raw_opc == 0xA6 || raw_opc == 0xA7 ||
                raw_opc == 0xAA || raw_opc == 0xAB ||
                raw_opc == 0xAC || raw_opc == 0xAD ||
                raw_opc == 0xAE || raw_opc == 0xAF)) {
            handler = execute_rep;
            dec->flags |= DECODED_FLAG_PARTIAL_PROGRESS;
        }
        else if (raw_opc == 0xA4 || raw_opc == 0xA5) {
            handler = execute_movs;
        }
        else if (raw_opc == 0xA6 || raw_opc == 0xA7) {
            handler = execute_cmps;
        }
        else if (raw_opc == 0xAA || raw_opc == 0xAB) {
            handler = execute_stos;
        }
        else if (raw_opc == 0xAC || raw_opc == 0xAD) {
            handler = execute_lods;
        }
        else if (raw_opc == 0xAE || raw_opc == 0xAF) {
            handler = execute_scas;
        }
        else if (raw_opc == 0xD7) {
            handler = execute_xlat;
        }
        else if ((handler = cpu_decoder_resolve_x87(buf, (size_t)fetched, raw_opc, prefix_len)) != NULL) {
            // handler set
        }
        else if (raw_opc == 0xF5 || raw_opc == 0xF8 || raw_opc == 0xF9 || raw_opc == 0xFC || raw_opc == 0xFD) {
            handler = execute_flags;
        }
        else if (raw_opc == 0x9C || raw_opc == 0x9D) {
            handler = execute_pushf;
        }
        else if (raw_opc == 0x9E || raw_opc == 0x9F) {
            handler = execute_lahf;
        }
        else if (raw_opc >= 0x50 && raw_opc <= 0x57) {
            handler = execute_push;
        }
        else if (raw_opc >= 0x58 && raw_opc <= 0x5F) {
            handler = execute_pop;
        }
        else if (raw_opc >= 0x00 && raw_opc <= 0x05) {
            handler = execute_add;
        }
        else if (raw_opc >= 0x08 && raw_opc <= 0x0D) {
            handler = execute_or;
        }
        else if (raw_opc >= 0x10 && raw_opc <= 0x15) {
            handler = execute_adc;
        }
        else if (raw_opc == 0x06 || raw_opc == 0x0E || raw_opc == 0x16 || raw_opc == 0x1E) {
            handler = execute_push;
        }
        else if (raw_opc == 0x07 || raw_opc == 0x17 || raw_opc == 0x1F) {
            handler = execute_pop;
        }
        else if (raw_opc >= 0x20 && raw_opc <= 0x25) {
            handler = execute_and;
        }
        else if (raw_opc >= 0x18 && raw_opc <= 0x1D) {
            handler = execute_sbb;
        }
        else if (raw_opc >= 0x28 && raw_opc <= 0x2D) {
            handler = execute_sub;
        }
        else if (raw_opc >= 0x30 && raw_opc <= 0x35) {
            handler = execute_xor;
        }
        else if (raw_opc >= 0x38 && raw_opc <= 0x3D) {
            handler = execute_cmp;
        }
        else if (raw_opc >= 0x70 && raw_opc <= 0x7F) {
            handler = execute_jcc;
        }
        else if ((raw_opc == 0x80 || raw_opc == 0x81 || raw_opc == 0x83) && group_reg != 0xFF) {
            switch (group_reg) {
            case 0: handler = execute_add; break;
            case 1: handler = execute_or; break;
            case 2: handler = execute_adc; break;
            case 3: handler = execute_sbb; break;
            case 4: handler = execute_and; break;
            case 5: handler = execute_sub; break;
            case 6: handler = execute_xor; break;
            case 7: handler = execute_cmp; break;
            default: dec->flags |= DECODED_FLAG_UD; return;
            }
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 1) {
            handler = execute_ror;
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 2) {
            handler = execute_rcl;
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 3) {
            handler = execute_rcr;
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 0) {
            handler = execute_rol;
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 4) {
            handler = execute_shl;
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 5) {
            handler = execute_shr;
        }
        else if ((raw_opc == 0xD0 || raw_opc == 0xD1 || raw_opc == 0xD2 || raw_opc == 0xD3 ||
            raw_opc == 0xC0 || raw_opc == 0xC1) && group2_reg == 7) {
            handler = execute_sar;
        }
        else if (raw_opc == 0xA8 || raw_opc == 0xA9 ||
            raw_opc == 0x84 || raw_opc == 0x85 ||
            ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 0)) {
            handler = execute_test;
        }
        else if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 2) {
            handler = execute_not;
        }
        else if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 3) {
            handler = execute_neg;
        }
        else if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 4) {
            handler = execute_mul;
        }
        else if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 5) {
            handler = execute_imul;
        }
        else if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 6) {
            handler = execute_div;
        }
        else if ((raw_opc == 0xF6 || raw_opc == 0xF7) && group3_reg == 7) {
            handler = execute_idiv;
        }
        else if (!cpu_is_64bit_code(ctx) && raw_opc >= 0x40 && raw_opc <= 0x47) {
            handler = execute_inc;
        }
        else if (!cpu_is_64bit_code(ctx) && raw_opc >= 0x48 && raw_opc <= 0x4F) {
            handler = execute_dec;
        }
        else if (raw_opc == 0xFE && fe_reg == 0) {
            handler = execute_inc;
        }
        else if (raw_opc == 0xFE && fe_reg == 1) {
            handler = execute_dec;
        }
        else if (raw_opc == 0x24 || raw_opc == 0x25) {
            handler = execute_and;
        }
        else if (raw_opc == 0x68 || raw_opc == 0x6A) {
            handler = execute_push;
        }
        else if (raw_opc == 0x69 || raw_opc == 0x6B) {
            handler = execute_imul;
        }
        else if (raw_opc == 0xE8 || raw_opc == 0x9A) {
            handler = execute_call;
        }
        else if (raw_opc >= 0xE0 && raw_opc <= 0xE3) {
            handler = execute_jcc;
        }
        else if (raw_opc == 0xEB || raw_opc == 0xE9 || raw_opc == 0xEA) {
            handler = execute_jmp;
        }
        else if (raw_opc == 0x8F) {
            handler = execute_pop;
        }
        else if (raw_opc == 0xFF && ff_reg != 0xFF) {
            switch (ff_reg) {
            case 0: handler = execute_inc; break;
            case 1: handler = execute_dec; break;
            case 2: case 3: handler = execute_call; break;
            case 4: case 5: handler = execute_jmp; break;
            case 6: handler = execute_push; break;
            default: dec->flags |= DECODED_FLAG_UD; return;
            }
        }
        else {
            dec->flags |= DECODED_FLAG_UD;
            return;
        }
    }

    dec->handler = handler;

    // Mark the entry as snapshot-free when the instruction is provably safe.
    // Only set the flag when no other side-channel forces us to keep the
    // scalar snapshot (UD / BP / vector dispatch handle their own state).
    if ((dec->flags & (DECODED_FLAG_UD | DECODED_FLAG_BP | DECODED_FLAG_DB | DECODED_FLAG_TOUCHES_VECTOR | DECODED_FLAG_INLINE_RET)) == 0) {
        if (cpu_decoder_classify_no_fault(buf, fetched, prefix_len, opc,
                group_reg, group3_reg, fe_reg, ff_reg, group2_reg)) {
            dec->flags |= DECODED_FLAG_NO_FAULT;
        }
    }

    // Try to install an execute_*_fast handler. The fast path runs a
    // pre-decoded DecodedInstruction stored in `dec->cached`, completely
    // skipping the legacy decode_* prefix scan + ModRM/operand-size
    // classification on every cache hit. We only install a fast handler when
    //   (1) the instruction shape is one we know does not depend on dynamic
    //       register state (no ModRM, or ModRM mod==3), so the cached
    //       DecodedInstruction stays valid across runs;
    //   (2) running the legacy decoder once does not raise an exception
    //       (we hide its CPU side effects behind a save/restore wrapper so
    //       a probe failure leaves no trace on guest state);
    //   (3) the instruction family has a paired execute_*_fast wrapper
    //       declared in its instruction header.
    cpu_decoder_try_attach_fast_handler(ctx, buf, fetched, prefix_len,
                                        opc, raw_opc, dec);
}

inline void cpu_decoder_try_attach_fast_handler(CPU_CONTEXT* ctx,
                                                const uint8_t* buf,
                                                int fetched,
                                                int prefix_len,
                                                uint16_t opc,
                                                uint8_t raw_opc,
                                                DecodedInst* dec)
{
    (void)buf;
    (void)fetched;
    (void)prefix_len;
    (void)opc;

    // Skip when the decoder already routed the entry to an inline path or to
    // the UD/BP escape; those paths never reach a function-pointer dispatch.
    if (dec->inline_kind != DECODED_INLINE_NONE) {
        return;
    }
    if (dec->flags & (DECODED_FLAG_UD | DECODED_FLAG_BP | DECODED_FLAG_DB | DECODED_FLAG_INLINE_RET)) {
        return;
    }
    if (dec->handler == NULL) {
        return;
    }
    if (dec->byte_count == 0) {
        return;
    }

    // MOV family — covers 88..8E reg-reg, A0..A3 mov AL/AX/RAX moffs, B0..BF
    // mov reg,imm, C6/C7 mov reg,imm. The shared executor body is invoked
    // through execute_mov_fast after stamping the cached prefix bits back
    // into ctx, so we re-use the entire legacy switch ladder unchanged.
    // The attach_one helper centralises the probe + safety check + payload
    // copy that every fast-handler installation needs. The legacy decode_*
    // helper is invoked through cpu_decoder_probe_with_helper so its
    // CPU_CONTEXT side effects are scrubbed regardless of outcome, and the
    // matching execute_*_fast wrapper is wired up only when the cached
    // DecodedInstruction is safe to replay across runs.
    auto attach_one = [&](DecodedInstruction(*decode_fn)(CPU_CONTEXT*, uint8_t*, size_t),
                          DecodedFastFn fast_fn) {
        DecodedInstruction probe = {};
        DecodedPrefixState probe_prefix = {};
        uint8_t probe_length = 0;
        if (cpu_decoder_probe_with_helper(ctx, dec->bytes, dec->byte_count,
                                          decode_fn, &probe, &probe_prefix,
                                          &probe_length)) {
            if (probe_length > 0 && cpu_decoder_dec_inst_safe_to_replay(&probe)) {
                dec->cached       = probe;
                dec->prefix       = probe_prefix;
                dec->length       = probe_length;
                dec->fast_handler = fast_fn;
            }
        }
    };

    // Same as attach_one, except the matching execute_*_fast handler must
    // recompute inst.mem_address from cached modrm/sib/disp on every replay
    // (not trust the decoder-time value, which captured live register
    // contents). This is what lets memory-form variants reach the fast path.
    auto attach_one_recompute_addr = [&](DecodedInstruction(*decode_fn)(CPU_CONTEXT*, uint8_t*, size_t),
                                         DecodedFastFn fast_fn) {
        DecodedInstruction probe = {};
        DecodedPrefixState probe_prefix = {};
        uint8_t probe_length = 0;
        if (cpu_decoder_probe_with_helper(ctx, dec->bytes, dec->byte_count,
                                          decode_fn, &probe, &probe_prefix,
                                          &probe_length)) {
            if (probe_length > 0 && cpu_decoder_dec_inst_safe_to_replay_recompute_addr(&probe)) {
                dec->cached       = probe;
                dec->prefix       = probe_prefix;
                dec->length       = probe_length;
                dec->fast_handler = fast_fn;
            }
        }
    };

    if (dec->handler == execute_mov) { attach_one_recompute_addr(decode_mov_instruction, execute_mov_fast); return; }
    if (dec->handler == execute_add) { attach_one_recompute_addr(decode_add_instruction, execute_add_fast); return; }
    if (dec->handler == execute_sub) { attach_one_recompute_addr(decode_sub_instruction, execute_sub_fast); return; }
    if (dec->handler == execute_cmp) { attach_one_recompute_addr(decode_cmp_instruction, execute_cmp_fast); return; }
    if (dec->handler == execute_test){ attach_one_recompute_addr(decode_test_instruction,execute_test_fast); return; }
    if (dec->handler == execute_and) { attach_one_recompute_addr(decode_and_instruction, execute_and_fast); return; }
    if (dec->handler == execute_or)  { attach_one_recompute_addr(decode_or_instruction,  execute_or_fast);  return; }
    if (dec->handler == execute_xor) { attach_one_recompute_addr(decode_xor_instruction, execute_xor_fast); return; }
    if (dec->handler == execute_inc) { attach_one(decode_inc_instruction, execute_inc_fast); return; }
    if (dec->handler == execute_dec) { attach_one(decode_dec_instruction, execute_dec_fast); return; }
    if (dec->handler == execute_push){ attach_one(decode_push_instruction,execute_push_fast); return; }
    if (dec->handler == execute_pop) { attach_one(decode_pop_instruction, execute_pop_fast); return; }
    if (dec->handler == execute_jmp) { attach_one(decode_jmp_instruction, execute_jmp_fast); return; }
    if (dec->handler == execute_call){ attach_one(decode_call_instruction,execute_call_fast); return; }
    if (dec->handler == execute_shl) { attach_one(decode_shl_instruction, execute_shl_fast); return; }
    if (dec->handler == execute_shr) { attach_one(decode_shr_instruction, execute_shr_fast); return; }
    if (dec->handler == execute_shld) { attach_one_recompute_addr(decode_shld_instruction, execute_shld_fast); return; }
    if (dec->handler == execute_shrd) { attach_one_recompute_addr(decode_shrd_instruction, execute_shrd_fast); return; }
    if (dec->handler == execute_sar) { attach_one(decode_sar_instruction, execute_sar_fast); return; }
    if (dec->handler == execute_rol) { attach_one(decode_rol_instruction, execute_rol_fast); return; }
    if (dec->handler == execute_ror) { attach_one(decode_ror_instruction, execute_ror_fast); return; }
    if (dec->handler == execute_rcl) { attach_one(decode_rcl_instruction, execute_rcl_fast); return; }
    if (dec->handler == execute_rcr) { attach_one(decode_rcr_instruction, execute_rcr_fast); return; }
    if (dec->handler == execute_adc) { attach_one(decode_adc_instruction, execute_adc_fast); return; }
    if (dec->handler == execute_sbb) { attach_one(decode_sbb_instruction, execute_sbb_fast); return; }
    if (dec->handler == execute_neg) { attach_one(decode_neg_instruction, execute_neg_fast); return; }
    if (dec->handler == execute_not) { attach_one(decode_not_instruction, execute_not_fast); return; }
    if (dec->handler == execute_xchg){ attach_one_recompute_addr(decode_xchg_instruction,execute_xchg_fast); return; }
    if (dec->handler == execute_movsx){attach_one_recompute_addr(decode_movsx_instruction,execute_movsx_fast);return; }
    if (dec->handler == execute_movzx){attach_one_recompute_addr(decode_movzx_instruction,execute_movzx_fast);return; }
    if (dec->handler == execute_movsxd){attach_one_recompute_addr(decode_movsxd_instruction,execute_movsxd_fast);return; }
    if (dec->handler == execute_cmovcc){attach_one_recompute_addr(decode_cmovcc_instruction,execute_cmovcc_fast);return; }
    if (dec->handler == execute_nop) { attach_one(decode_nop_instruction, execute_nop_fast); return; }
    if (dec->handler == execute_mul) { attach_one(decode_mul_instruction, execute_mul_fast); return; }
    if (dec->handler == execute_imul){ attach_one(decode_imul_instruction,execute_imul_fast); return; }
    if (dec->handler == execute_div) { attach_one(decode_div_instruction, execute_div_fast); return; }
    if (dec->handler == execute_idiv){ attach_one(decode_idiv_instruction,execute_idiv_fast); return; }
    if (dec->handler == execute_setcc){attach_one(decode_setcc_instruction,execute_setcc_fast);return; }
    if (dec->handler == execute_cdq) { attach_one(decode_cdq_instruction, execute_cdq_fast); return; }
    if (dec->handler == execute_cbw) { attach_one(decode_cbw_instruction, execute_cbw_fast); return; }
    if (dec->handler == execute_leave){attach_one(decode_leave_instruction,execute_leave_fast);return; }
    if (dec->handler == execute_lahf){ attach_one(decode_lahf_instruction,execute_lahf_fast); return; }
    if (dec->handler == execute_flags){attach_one(decode_flags_instruction,execute_flags_fast);return; }
    if (dec->handler == execute_bswap){attach_one(decode_bswap_instruction,execute_bswap_fast);return; }
    if (dec->handler == execute_pushf){attach_one(decode_pushf_instruction,execute_pushf_fast);return; }
    if (dec->handler == execute_movs){ attach_one(decode_movs_instruction,execute_movs_fast); return; }
    if (dec->handler == execute_cmps){ attach_one(decode_cmps_instruction,execute_cmps_fast); return; }
    if (dec->handler == execute_stos){ attach_one(decode_stos_instruction,execute_stos_fast); return; }
    if (dec->handler == execute_lods){ attach_one(decode_lods_instruction,execute_lods_fast); return; }
    if (dec->handler == execute_scas){ attach_one(decode_scas_instruction,execute_scas_fast); return; }
    if (dec->handler == execute_xlat){ attach_one(decode_xlat_instruction,execute_xlat_fast); return; }
    if (dec->handler == execute_pause){attach_one(decode_pause_instruction,execute_pause_fast);return; }
    if (dec->handler == execute_bt)   { attach_one(decode_bt_instruction,   execute_bt_fast);   return; }
    if (dec->handler == execute_bsf)  { attach_one(decode_bsf_instruction,  execute_bsf_fast);  return; }
    if (dec->handler == execute_xadd) { attach_one_recompute_addr(decode_xadd_instruction, execute_xadd_fast); return; }
    if (dec->handler == execute_cmpxchg){attach_one_recompute_addr(decode_cmpxchg_instruction,execute_cmpxchg_fast);return; }
    if (dec->handler == execute_cpuid){ attach_one(decode_cpuid_instruction,execute_cpuid_fast);return; }
    if (dec->handler == execute_rdtsc){ attach_one(decode_rdtsc_instruction,execute_rdtsc_fast);return; }
    if (dec->handler == execute_rdtscp){attach_one(decode_rdtscp_instruction,execute_rdtscp_fast);return; }
    if (dec->handler == execute_rdpmc){ attach_one(decode_rdpmc_instruction,execute_rdpmc_fast);return; }
    if (dec->handler == execute_rdseed){ attach_one(decode_rdseed_instruction,execute_rdseed_fast);return; }
    if (dec->handler == execute_xgetbv){attach_one(decode_xgetbv_instruction,execute_xgetbv_fast);return; }
    if (dec->handler == execute_rdpid){ attach_one(decode_rdpid_instruction,execute_rdpid_fast);return; }
    if (dec->handler == execute_fsgsbase){ attach_one(decode_fsgsbase_instruction,execute_fsgsbase_fast);return; }
    if (dec->handler == execute_serialize){ attach_one(decode_serialize_instruction,execute_serialize_fast);return; }
    if (dec->handler == execute_popcnt){attach_one(decode_popcnt_instruction,execute_popcnt_fast);return; }
    if (dec->handler == execute_endbr){ attach_one(decode_endbr_instruction,execute_endbr_fast);return; }
    if (dec->handler == execute_xorps){ attach_one(decode_xorps_instruction,execute_xorps_fast);return; }
    if (dec->handler == execute_movaps){attach_one(decode_movaps_instruction,execute_movaps_fast);return; }
    if (dec->handler == execute_movups){attach_one(decode_movups_instruction,execute_movups_fast);return; }
    if (dec->handler == execute_movdq){ attach_one(decode_movdq_instruction_no_aux,execute_movdq_fast);return; }
    if (dec->handler == execute_movss){ attach_one(decode_movss_instruction,execute_movss_fast);return; }
    if (dec->handler == execute_enter){ attach_one(decode_enter_instruction,execute_enter_fast);return; }
    if (dec->handler == execute_rep)  { attach_one(decode_rep_for_fast,     execute_rep_fast);  return; }
    if (dec->handler == execute_sse2_int_logic){ attach_one(decode_sse2_int_logic_instruction_no_aux,execute_sse2_int_logic_fast);return; }
    if (dec->handler == execute_sse2_mov_int)  { attach_one(decode_sse2_mov_int_instruction_no_aux,execute_sse2_mov_int_fast);return; }
    if (dec->handler == execute_sse2_mov_pd)   { attach_one(decode_sse2_mov_pd_instruction_no_aux,execute_sse2_mov_pd_fast);return; }
    if (dec->handler == execute_sse2_int_arith){ attach_one(decode_sse2_int_arith_instruction_no_aux,execute_sse2_int_arith_fast);return; }
    if (dec->handler == execute_sse2_int_cmp)  { attach_one(decode_sse2_int_cmp_instruction_no_aux,execute_sse2_int_cmp_fast);return; }
    if (dec->handler == execute_pshufb)        { attach_one(decode_pshufb_instruction_no_aux,execute_pshufb_fast);return; }
    if (dec->handler == execute_sse2_shuffle)  { attach_one(decode_sse2_shuffle_instruction_no_aux,execute_sse2_shuffle_fast);return; }
    if (dec->handler == execute_sse_shuffle)   { attach_one(decode_sse_shuffle_instruction_no_aux,execute_sse_shuffle_fast);return; }
    if (dec->handler == execute_sse_logic)     { attach_one(decode_sse_logic_instruction_no_aux,execute_sse_logic_fast);return; }
    if (dec->handler == execute_sse_arith)     { attach_one(decode_sse_arith_instruction_no_aux,execute_sse_arith_fast);return; }
    if (dec->handler == execute_sse2_arith_pd) { attach_one(decode_sse2_arith_pd_instruction_no_aux,execute_sse2_arith_pd_fast);return; }
    if (dec->handler == execute_sse2_logic_pd) { attach_one(decode_sse2_logic_pd_instruction_no_aux,execute_sse2_logic_pd_fast);return; }
    if (dec->handler == execute_sse_cmp)       { attach_one(decode_sse_cmp_instruction_no_aux,execute_sse_cmp_fast);return; }
    if (dec->handler == execute_sse2_cmp_pd)   { attach_one(decode_sse2_cmp_pd_instruction_no_aux,execute_sse2_cmp_pd_fast);return; }
    if (dec->handler == execute_sse2_pack)     { attach_one(decode_sse2_pack_instruction_no_aux,execute_sse2_pack_fast);return; }
    if (dec->handler == execute_sse2_shift)    { attach_one(decode_sse2_shift_instruction_no_aux,execute_sse2_shift_fast);return; }
    if (dec->handler == execute_sse_math)      { attach_one(decode_sse_math_instruction_no_aux,execute_sse_math_fast);return; }
    if (dec->handler == execute_sse2_math_pd)  { attach_one(decode_sse2_math_pd_instruction_no_aux,execute_sse2_math_pd_fast);return; }
    if (dec->handler == execute_sse_convert)   { attach_one(decode_sse_convert_instruction_no_aux,execute_sse_convert_fast);return; }
    if (dec->handler == execute_sse2_convert)  { attach_one(decode_sse2_convert_instruction_no_aux,execute_sse2_convert_fast);return; }
    if (dec->handler == execute_sse2_scalar_convert) { attach_one(decode_sse2_scalar_convert_instruction_no_aux,execute_sse2_scalar_convert_fast);return; }
    if (dec->handler == execute_sse_mov_misc)  { attach_one(decode_sse_mov_misc_instruction,execute_sse_mov_misc_fast);return; }

    (void)raw_opc;
}
