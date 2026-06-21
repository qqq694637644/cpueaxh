#pragma once

// cpu/executor.hpp - Instruction fetch-decode-dispatch main loop.
//
// The instruction stream is decoded once per static program counter and the
// resulting DecodedInst is cached by RIP, code-version and CPU-mode key (see
// cpu/inst_cache.hpp). On a hit we skip prefix scanning, opcode classification
// and the long if/else dispatch ladder, going straight to the cached handler.

#include "core.hpp"
#include "inst_cache.hpp"
#include "decoder.hpp"

// Maximum bytes we fetch ahead per instruction (Intel max is 15 bytes). The
// canonical definition lives in dispatch_helpers.hpp; we re-affirm the value
// here so other translation units that include just executor.hpp keep working.
#ifndef MAX_INST_FETCH
#define MAX_INST_FETCH 15
#endif

static_assert(MAX_INST_FETCH == DECODED_INST_MAX_BYTES,
    "MAX_INST_FETCH and DECODED_INST_MAX_BYTES must match");

// Result codes for cpu_step / cpu_run
#define CPU_STEP_OK        0
#define CPU_STEP_HALT      1   // HLT encountered
#define CPU_STEP_FETCH_ERR 2   // no bytes readable at rip
#define CPU_STEP_UD        3   // unrecognised opcode
#define CPU_STEP_EXCEPTION 4   // pending CPU exception

#ifndef CPUEAXH_STRICT_INTERNAL
#define CPUEAXH_STRICT_INTERNAL 1
#endif

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Fetch up to MAX_INST_FETCH bytes from virtual memory starting at addr.
// Returns the number of bytes actually read. Intentionally does NOT trigger
// MEM_FETCH hooks: the prefetch buffer is up to 15 bytes wide, far larger
// than the 1..11 bytes any single instruction actually consumes, so notifying
// at fetch time would over-fire on the bytes that belong to the *next*
// instruction. The notify pass below (cpu_executor_notify_fetched_bytes)
// runs once we know the decoder's true consumption count and notifies only
// the bytes that this instruction is actually executing.
static int fetch_instruction_bytes(CPU_CONTEXT* ctx, uint64_t addr, uint8_t* buf) {
    if (!ctx || !buf || cpu_has_exception(ctx)) {
        return 0;
    }

    if (cpu_can_use_page_fast_path(ctx)) {
        int n = 0;
        while (n < MAX_INST_FETCH) {
            const uint64_t current_address = addr + (uint64_t)n;
            uint8_t* ptr = NULL;
            if (cpu_resolve_memory_access(ctx, current_address, MM_PROT_EXEC, &ptr, current_address, 1, 0) != MM_ACCESS_OK) {
                break;
            }

            size_t chunk = cpu_page_bytes_remaining(current_address);
            const size_t remaining = (size_t)(MAX_INST_FETCH - n);
            if (chunk > remaining) {
                chunk = remaining;
            }

            CPUEAXH_MEMCPY(buf + n, ptr, chunk);
            n += (int)chunk;
        }
        return n;
    }

    // Slow path (resolver doesn't allow a contiguous host pointer): walk
    // the instruction byte-by-byte. We do NOT notify MEM_FETCH here for the
    // same reason as the fast path above -- the post-decode notify pass
    // (cpu_executor_notify_fetched_bytes) fires hooks once the decoder has
    // told us how many bytes the instruction actually consumed.
    int n = 0;
    for (int i = 0; i < MAX_INST_FETCH; i++) {
        uint8_t* ptr = NULL;
        if (cpu_resolve_memory_access(ctx, addr + (uint64_t)i, MM_PROT_EXEC, &ptr, addr + (uint64_t)i, 1, 0) != MM_ACCESS_OK) {
            break;
        }
        buf[i] = *ptr;
        n++;
    }
    return n;
}

// Fire MEM_FETCH hooks for the bytes the decoder actually consumed for this
// instruction. Called once we have a resolved DecodedInst (either from cache
// or freshly decoded) and right before the handler runs, so the user-visible
// notification order matches "fetch -> read/write" expected by Unicorn-style
// hosts. Bytes are read out of decoded->bytes (the cache entry's own copy),
// which is what the executor will actually execute against.
//
// IMPORTANT: byte_count records how many bytes were COPIED into the cache
// (up to MAX_INST_FETCH=15, the prefetch-buffer width), NOT how many bytes
// the decoded instruction actually executes. The "true" length lives in
// `length`, which the fast-handler attach pass and the inline RET/Jcc path
// stamp from the underlying decode_*_instruction probe. Using byte_count
// here would re-fire MEM_FETCH for bytes belonging to the *next* instruction,
// which is exactly the bug this notify pass exists to fix.
//
// A cached handler with `length <= 0` is an incomplete decode payload. Strict
// development builds fail that state instead of falling back to byte_count and
// hiding the decode bug.
static inline void cpu_executor_notify_fetched_bytes(CPU_CONTEXT* ctx, uint64_t rip, const DecodedInst* decoded) {
    if (!cpu_has_hook_type(ctx, CPUEAXH_HOOK_MEM_FETCH)) {
        return;
    }
    int count = (int)decoded->length;
    if (count <= 0) {
#if CPUEAXH_STRICT_INTERNAL
        raise_ud_ctx(ctx);
        return;
#else
        count = (int)decoded->byte_count;
#endif
    }
    if (count > (int)decoded->byte_count) {
        count = (int)decoded->byte_count;
    }
    for (int i = 0; i < count; i++) {
        cpu_notify_memory_hook(ctx, CPUEAXH_HOOK_MEM_FETCH, rip + (uint64_t)i, 1, decoded->bytes[i]);
    }
}

// Inline RET near (0xC3) implementation. Mirrors the legacy fast path so
// hot return-heavy workloads avoid an extra function call indirection.
static inline void cpu_executor_inline_ret_no_imm(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    ctx->last_inst_size = (int)dec->near_ret_prefix_len + 1;
    if (dec->near_ret_op_size == 16) {
        uint16_t target = pop_value16(ctx);
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_assign_rip_checked(ctx, target, 16);
    }
    else if (dec->near_ret_op_size == 32) {
        uint32_t target = pop_value32(ctx);
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_assign_rip_checked(ctx, target, 32);
    }
    else {
        uint64_t target = pop_value64(ctx);
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_assign_rip_checked(ctx, target, 64);
    }
}

// Inline RET near with imm16 release (0xC2). Same rationale as above.
static inline void cpu_executor_inline_ret_imm16(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    const int near_ret_prefix_len = (int)dec->near_ret_prefix_len;
    const int operand_size = (int)dec->near_ret_op_size;
    const uint16_t imm16 = dec->inline_imm16;

    ctx->last_inst_size = near_ret_prefix_len + 3;
    if (operand_size == 16) {
        uint16_t target = pop_value16(ctx);
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_assign_rip_checked(ctx, target, 16);
    }
    else if (operand_size == 32) {
        uint32_t target = pop_value32(ctx);
        if (cpu_has_exception(ctx)) {
            return;
        }
        cpu_assign_rip_checked(ctx, target, 32);
    }
    else {
        uint64_t target = pop_value64(ctx);
        if (cpu_has_exception(ctx)) {
            return;
        }
        if (!cpu_assign_rip_checked(ctx, target, 64)) {
            return;
        }
    }

    if (get_stack_addr_size(ctx) == 64) {
        ctx->regs[REG_RSP] += imm16;
    }
    else if (get_stack_addr_size(ctx) == 32) {
        uint32_t esp = (uint32_t)(ctx->regs[REG_RSP] & 0xFFFFFFFFu);
        esp += imm16;
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFFFFFULL) | esp;
    }
    else {
        uint16_t sp = (uint16_t)(ctx->regs[REG_RSP] & 0xFFFFu);
        sp = (uint16_t)(sp + imm16);
        ctx->regs[REG_RSP] = (ctx->regs[REG_RSP] & ~0xFFFFULL) | sp;
    }
}

// Evaluate a Jcc condition code (low 4 bits of opcode) directly from RFLAGS.
// Shared between the short and near long-mode inline paths.
static inline bool cpu_executor_eval_jcc_condition(uint64_t flags, uint8_t cond) {
    const bool cf = (flags & RFLAGS_CF) != 0;
    const bool zf = (flags & RFLAGS_ZF) != 0;
    const bool sf = (flags & RFLAGS_SF) != 0;
    const bool of = (flags & RFLAGS_OF) != 0;
    const bool pf = (flags & RFLAGS_PF) != 0;
    switch (cond) {
    case 0x0: return of;                    // JO
    case 0x1: return !of;                   // JNO
    case 0x2: return cf;                    // JB
    case 0x3: return !cf;                   // JAE
    case 0x4: return zf;                    // JE
    case 0x5: return !zf;                   // JNE
    case 0x6: return cf || zf;              // JBE
    case 0x7: return !cf && !zf;            // JA
    case 0x8: return sf;                    // JS
    case 0x9: return !sf;                   // JNS
    case 0xA: return pf;                    // JP
    case 0xB: return !pf;                   // JNP
    case 0xC: return sf != of;              // JL
    case 0xD: return sf == of;              // JGE
    case 0xE: return zf || (sf != of);      // JLE
    case 0xF: return !zf && (sf == of);     // JG
    default:
        CPUEAXH_UNREACHABLE();
        return false;
    }
}

// Inline short Jcc rel8 evaluation in 64-bit long mode. The decoder pre-stamps
// the condition code and signed displacement; we re-derive the boolean from
// RFLAGS and either fall through (rip += 2) or take the branch.
static inline void cpu_executor_inline_jcc_short_longmode(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    const uint64_t next_rip = ctx->rip + 2u;
    if (cpu_executor_eval_jcc_condition(ctx->rflags, dec->inline_jcc_cond)) {
        cpu_assign_rip_checked(ctx, next_rip + (uint64_t)(int64_t)dec->inline_jcc_disp, 64);
    } else {
        cpu_assign_rip_checked(ctx, next_rip, 64);
    }
    ctx->last_inst_size = 2;
}

// Inline near Jcc rel32 evaluation in 64-bit long mode (0x0F 0x80..0x8F).
static inline void cpu_executor_inline_jcc_near_longmode(CPU_CONTEXT* ctx, const DecodedInst* dec) {
    const uint64_t next_rip = ctx->rip + 6u;
    if (cpu_executor_eval_jcc_condition(ctx->rflags, dec->inline_jcc_cond)) {
        cpu_assign_rip_checked(ctx, next_rip + (uint64_t)(int64_t)dec->inline_jcc_disp32, 64);
    } else {
        cpu_assign_rip_checked(ctx, next_rip, 64);
    }
    ctx->last_inst_size = 6;
}

// Execute one already-resolved DecodedInst. Shared by cpu_step_with_prefetch
// (which performs its own cache lookup / decode) and cpu_step_decoded (the
// outer-loop fast path that has already validated a cache hit and wants to
// avoid repeating the lookup work). `decoded` MUST point to a non-empty
// DecodedInst whose `bytes` are still valid for the lifetime of this call.
// `rip_pre` is the value of ctx->rip at entry; passed in so we don't have to
// re-read it after the handler may have mutated rip.
static inline int cpu_step_dispatch_decoded(CPU_CONTEXT* ctx, DecodedInst* decoded, uint64_t rip_pre) {
    CPU_SCALAR_SNAPSHOT saved_scalar;
    CPU_VECTOR_SNAPSHOT saved_vector;
    bool scalar_snapshot_valid = false;
    bool vector_snapshot_valid = false;
    int result_code = CPU_STEP_OK;

    // Notify MEM_FETCH for exactly the bytes the decoder consumed, before
    // any handler runs. This restores the Unicorn-style "fetch then
    // read/write" ordering even when:
    //   * the prefetch buffer over-read past the instruction (page fast
    //     path), and
    //   * cache hits skipped fetch_instruction_bytes entirely.
    cpu_executor_notify_fetched_bytes(ctx, rip_pre, decoded);

    {
        uint8_t* exec_bytes = decoded->bytes;
        int exec_fetched = (int)decoded->byte_count;

        // Segment override was resolved by the decoder and stamped into the
        // cache entry; no need to rescan prefix bytes on every step.
        ctx->segment_override = (int)decoded->segment_override;

        // Quick #UD / #BP exits captured at decode time.
        if (decoded->flags & DECODED_FLAG_UD) {
            raise_ud_ctx(ctx);
            result_code = CPU_STEP_UD;
            goto cpu_step_finish;
        }
        if (decoded->flags & DECODED_FLAG_BP) {
            raise_bp_ctx(ctx);
            result_code = CPU_STEP_EXCEPTION;
            goto cpu_step_finish;
        }
        if (decoded->flags & DECODED_FLAG_DB) {
            raise_db_ctx(ctx);
            result_code = CPU_STEP_EXCEPTION;
            goto cpu_step_finish;
        }

        // Snapshot policy:
        //   * scalar snapshot is taken unless the decoder proved the handler
        //     cannot fault (DECODED_FLAG_NO_FAULT). This sidesteps a ~440 byte
        //     memcpy on every ALU / control-flow step.
        //   * vector snapshot is taken only when the handler may touch SIMD
        //     state, gated by DECODED_FLAG_TOUCHES_VECTOR.
        if (!(decoded->flags & DECODED_FLAG_NO_FAULT)) {
            cpu_capture_scalar_snapshot(&saved_scalar, ctx);
            scalar_snapshot_valid = true;
        }
        if (decoded->flags & DECODED_FLAG_TOUCHES_VECTOR) {
            cpu_capture_vector_snapshot(&saved_vector, ctx);
            vector_snapshot_valid = true;
        }

        // Inline forms (RET / short Jcc) avoid a function pointer call
        // entirely, dramatically tightening the inner loop on hot return /
        // branch paths.
        if (decoded->flags & DECODED_FLAG_INLINE_RET) {
            if (decoded->inline_kind == DECODED_INLINE_RET_NEAR_NO_IMM) {
                cpu_executor_inline_ret_no_imm(ctx, decoded);
            }
            else if (decoded->inline_kind == DECODED_INLINE_RET_NEAR_IMM16) {
                cpu_executor_inline_ret_imm16(ctx, decoded);
            }
            goto cpu_step_finish;
        }
        if (decoded->inline_kind == DECODED_INLINE_JCC_SHORT_LONGMODE) {
            cpu_executor_inline_jcc_short_longmode(ctx, decoded);
            goto cpu_step_finish;
        }
        if (decoded->inline_kind == DECODED_INLINE_JCC_NEAR_LONGMODE) {
            cpu_executor_inline_jcc_near_longmode(ctx, decoded);
            goto cpu_step_finish;
        }

        // Prefer the cached fast handler when available: it consumes the
        // pre-decoded DecodedInstruction stored in `decoded->cached`, so the
        // legacy decode_* prefix scan + ModRM/operand-size classification
        // never runs on cache hits. We still pass through `decoded->handler`
        // for instruction families that do not yet have a fast wrapper, and
        // for shapes (e.g. memory-form ModRM) where the cached payload would
        // depend on dynamic register state and is therefore unsafe to reuse.
        // The fast handler is responsible for setting ctx->last_inst_size
        // (typically from dec->length), so the post-handler RIP advance
        // below sees the correct value without re-running the decoder.
        if (decoded->fast_handler) {
            decoded->fast_handler(ctx, decoded);
        }
        else if (decoded->handler) {
            decoded->handler(ctx, exec_bytes, (size_t)exec_fetched);
        }
        // handler == NULL && no flags: matches the legacy "0x0F1E NOP-like"
        // fallthrough where the original dispatcher invoked nothing yet still
        // advanced past the instruction using last_inst_size.
    }

    if (cpu_has_exception(ctx)) {
        goto cpu_step_finish;
    }

    if (!(decoded->flags & DECODED_FLAG_BRANCH)) {
        ctx->rip = rip_pre + (uint64_t)ctx->last_inst_size;
    }

cpu_step_finish:
    if (cpu_has_exception(ctx)) {
        CPU_EXCEPTION_STATE exception_state = ctx->exception;
        uint64_t saved_cr2 = ctx->control_regs[REG_CR2];
        if (scalar_snapshot_valid) {
            if (decoded->flags & DECODED_FLAG_PARTIAL_PROGRESS) {
                cpu_restore_decode_transient_snapshot(ctx, &saved_scalar);
            }
            else {
                cpu_restore_scalar_snapshot(ctx, &saved_scalar);
                if (vector_snapshot_valid) {
                    cpu_restore_vector_snapshot(ctx, &saved_vector);
                }
            }
        }
        ctx->exception = exception_state;
        ctx->control_regs[REG_CR2] = saved_cr2;
        ctx->segment_override = -1;
        return CPU_STEP_EXCEPTION;
    }

    ctx->segment_override = -1;
    return result_code;
}

// ---------------------------------------------------------------------------
// cpu_step - execute one instruction at ctx->rip
// ---------------------------------------------------------------------------
static int cpu_step_with_prefetch(CPU_CONTEXT* ctx, const uint8_t* prefetched_bytes, int prefetched_size) {
    if (cpu_has_exception(ctx)) {
        return CPU_STEP_EXCEPTION;
    }

    // Skip cpu_clear_exception(): we just verified exception.code is NONE, so
    // resetting it (along with error_code) is a wasted store on the hot path.
    // cpu_raise_exception always overwrites error_code when it fires, so a
    // stale value never reaches the guest.

    const uint64_t rip_pre = ctx->rip;

    // Resolve the cached decoded instruction (or decode one if absent /
    // version-mismatched) before touching scratch state.
    //
    // Cache is consulted on BOTH paths (with and without a prefetched
    // buffer). This used to be skipped when prefetched_bytes was non-NULL
    // ("might have been mutated by hook/escape callbacks"), but in practice:
    //   * cache hits compare on (rip, code_version, mode), not on the
    //     prefetched bytes themselves;
    //   * code_version is bumped by the memory manager on any code-page
    //     mutation, so SMC and remap correctly invalidate the cache;
    //   * the executor below uses decoded->bytes (the cache entry's own
    //     copy), not the caller-supplied prefetched_bytes, so even a callback
    //     that scribbles on its bytes argument cannot perturb dispatch.
    // Letting hook/escape paths share the cache is the difference between
    // ~0 Msteps/s and full fast-path throughput when escapes are registered
    // but rarely fire (the dominant pattern for whole-binary emulation).
    InstCache* const cache = ctx->inst_cache;
    const InstCacheModeKey mode_key = cpu_inst_cache_mode_key(ctx);
    InstCacheEntry* hit = NULL;
    if (cache) {
        // Inline lookup: cache->slots is guaranteed non-NULL when ctx->inst_cache
        // is set (cpueaxh_open enforces this) so the null guards inside
        // cpu_inst_cache_lookup are dead code on the hot path.
        InstCacheEntry* const slot = &cache->slots[cpu_inst_cache_index(rip_pre)];
        const uint64_t current_version = ctx->mem_mgr->code_version;
        const bool is_hit = (slot->tag == rip_pre) &&
                            (slot->version == current_version) &&
                            (slot->mode.bits == mode_key.bits);
        if (is_hit) {
            hit = slot;
        }
#if defined(CPUEAXH_INST_CACHE_STATS)
        if (is_hit) cache->hits++; else cache->misses++;
#endif
    }

    DecodedInst local_decoded;
    DecodedInst* decoded = NULL;
    uint8_t local_buf[MAX_INST_FETCH];
    int fetched = 0;

    if (hit) {
        decoded = &hit->decoded;
        fetched = (int)decoded->byte_count;
    }
    else {
        if (prefetched_bytes && prefetched_size > 0) {
            fetched = prefetched_size > MAX_INST_FETCH ? MAX_INST_FETCH : prefetched_size;
            CPUEAXH_MEMCPY(local_buf, prefetched_bytes, (size_t)fetched);
        }
        else {
            fetched = fetch_instruction_bytes(ctx, rip_pre, local_buf);
        }
        if (fetched == 0) {
            ctx->segment_override = -1;
            return cpu_has_exception(ctx) ? CPU_STEP_EXCEPTION : CPU_STEP_FETCH_ERR;
        }

        cpu_decode_instruction(ctx, local_buf, fetched, &local_decoded);

        if (cache && local_decoded.byte_count != 0) {
            // Promote the freshly decoded instruction into the cache so the
            // next visit at this RIP skips the entire decode work, regardless
            // of whether the bytes came from a prefetch or a direct fetch.
            InstCacheEntry* slot = cpu_inst_cache_reserve(cache, rip_pre);
            if (slot) {
                slot->decoded = local_decoded;
                cpu_inst_cache_commit(slot, ctx->mem_mgr, rip_pre, mode_key);
                decoded = &slot->decoded;
            }
            else {
                decoded = &local_decoded;
            }
        }
        else {
            decoded = &local_decoded;
        }
    }

    if (decoded->byte_count == 0) {
        ctx->segment_override = -1;
        return cpu_has_exception(ctx) ? CPU_STEP_EXCEPTION : CPU_STEP_FETCH_ERR;
    }

    return cpu_step_dispatch_decoded(ctx, decoded, rip_pre);
}

// Execute the instruction whose decoded form already lives in `slot`. Used by
// the executor outer loop's KNOWN_NOT_ESCAPE fast path: it has already done
// the (rip, version, mode) tag comparison to validate the slot, so repeating
// the lookup inside cpu_step_with_prefetch is wasted work. The caller MUST
// have verified the slot is a hit at the *current* ctx->rip; the dispatcher
// trusts that and goes straight to handler invocation.
//
// Compatible-by-design: writes a single ctx->rip read (rip_pre) and then
// hands off to the same cpu_step_dispatch_decoded path that cpu_step uses,
// so behaviour is identical to "cache-hit branch of cpu_step" with one fewer
// lookup.
static inline int cpu_step_decoded(CPU_CONTEXT* ctx, InstCacheEntry* slot) {
    if (cpu_has_exception(ctx)) {
        return CPU_STEP_EXCEPTION;
    }
    return cpu_step_dispatch_decoded(ctx, &slot->decoded, ctx->rip);
}

int cpu_step(CPU_CONTEXT* ctx) {
    return cpu_step_with_prefetch(ctx, NULL, 0);
}

// ---------------------------------------------------------------------------
// cpu_run - run until HLT, exception, or max_steps
// ---------------------------------------------------------------------------

// max_steps == 0 means unlimited (run until HLT or exception propagates out).
// Returns the number of instructions successfully executed.
uint64_t cpu_run(CPU_CONTEXT* ctx, uint64_t max_steps) {
    uint64_t count = 0;
    for (;;) {
        if (max_steps != 0 && count >= max_steps) break;
        int result = cpu_step_with_prefetch(ctx, NULL, 0);
        if (result == CPU_STEP_HALT) break;
        if (result != CPU_STEP_OK)  break;
        count++;
    }
    return count;
}
