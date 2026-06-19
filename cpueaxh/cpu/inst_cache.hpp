#pragma once

// cpu/inst_cache.hpp - Direct-mapped cache of pre-decoded instructions keyed
// by RIP. Cache entries are validated against a monotonic code-version counter
// maintained by the memory manager so that any change to the executable view
// of guest memory transparently invalidates stale entries.
//
// The cache itself is allocated through CPUEAXH_ALLOC_ZEROED so it works in
// both user-mode (calloc) and kernel-mode (non-paged pool) builds without any
// platform-specific code in the hot path.

#pragma once

#include "../cpueaxh_platform.hpp"
#include "../memory/manager.hpp"
#include "def.h"
#include "decoded.hpp"

// Number of cache slots. Power of two so we can mask instead of modulo.
// 32K * sizeof(InstCacheEntry) ~= 1.6 MB which is acceptable for non-paged
// pool allocations on Windows kernels.
#define CPU_INST_CACHE_BITS 15u
#define CPU_INST_CACHE_SIZE (1u << CPU_INST_CACHE_BITS)
#define CPU_INST_CACHE_MASK (CPU_INST_CACHE_SIZE - 1u)

// Snapshot of CPU state bits that affect how instruction bytes are decoded.
// Stored alongside each cache entry so that mode switches (e.g. transition
// between long mode and compatibility 32-bit code) automatically miss the
// cache instead of executing a stale handler.
struct InstCacheModeKey {
    uint8_t bits;
};

#define INST_CACHE_MODE_LONG_MODE_ACTIVE 0x1u
#define INST_CACHE_MODE_CS_LONG_MODE     0x2u
#define INST_CACHE_MODE_CS_DB            0x4u

// Slow path: actually compute the mode-key bits from CPU state. Pulled out
// so the inline accessor stays small enough for the compiler to fold the
// cache-hit branch into the caller (cpu_step / executor outer loop).
inline uint8_t cpu_inst_cache_compute_mode_key_bits(const CPU_CONTEXT* ctx) {
    uint8_t bits = 0;
    if (ctx->long_mode_active) {
        bits |= INST_CACHE_MODE_LONG_MODE_ACTIVE;
    }
    if (ctx->cs.descriptor.long_mode) {
        bits |= INST_CACHE_MODE_CS_LONG_MODE;
    }
    if (ctx->cs.descriptor.db) {
        bits |= INST_CACHE_MODE_CS_DB;
    }
    return bits;
}

inline InstCacheModeKey cpu_inst_cache_mode_key(CPU_CONTEXT* ctx) {
    InstCacheModeKey key = {};
    if (!ctx) {
        return key;
    }
    // The cache invariant: any path that mutates long_mode_active or
    // cs.descriptor.{long_mode,db} clears cached_mode_key_valid. So if the
    // valid flag is set, the cached bits are still authoritative for the
    // current CPU state.
    if (ctx->cached_mode_key_valid) {
        key.bits = ctx->cached_mode_key_bits;
        return key;
    }
    const uint8_t bits = cpu_inst_cache_compute_mode_key_bits(ctx);
    ctx->cached_mode_key_bits = bits;
    ctx->cached_mode_key_valid = 1;
    key.bits = bits;
    return key;
}

// Read-only overload: use when the caller cannot mutate the context (e.g.
// debugger queries). Always recomputes; never primes the cache.
inline InstCacheModeKey cpu_inst_cache_mode_key_const(const CPU_CONTEXT* ctx) {
    InstCacheModeKey key = {};
    if (!ctx) {
        return key;
    }
    key.bits = cpu_inst_cache_compute_mode_key_bits(ctx);
    return key;
}

// Mark the cached mode-key as stale. Call this from code sites that change
// long_mode_active or cs.descriptor.{long_mode,db}. Cheap (a single byte
// store) so it's safe to sprinkle near such writes; the recompute on the
// next read handles the rest.
inline void cpu_invalidate_mode_key_cache(CPU_CONTEXT* ctx) {
    if (ctx) {
        ctx->cached_mode_key_valid = 0;
    }
}

struct InstCacheEntry {
    uint64_t        tag;       // Guest RIP. 0 means "empty slot".
    uint64_t        version;   // Snapshot of MEMORY_MANAGER::code_version at insert.
    InstCacheModeKey mode;     // CPU mode bits captured at decode time.
    DecodedInst     decoded;   // Pre-decoded instruction.
};

struct InstCache {
    InstCacheEntry* slots; // CPU_INST_CACHE_SIZE entries.
#if defined(CPUEAXH_INST_CACHE_STATS)
    // Lightweight counters compiled in only when the host bench/instrumentation
    // requests them. Kept out of the default build so the hot lookup path stays
    // a tag/version/mode compare without any extra memory traffic.
    uint64_t        hits;
    uint64_t        misses;
#endif
};

inline uint32_t cpu_inst_cache_index(uint64_t rip) {
    // Lightweight mixer: most call/jmp targets share low bits, so fold the
    // upper RIP bits into the index to spread aliased addresses.
    uint64_t mixed = rip ^ (rip >> 13) ^ (rip >> 27);
    return (uint32_t)(mixed & CPU_INST_CACHE_MASK);
}

inline bool cpu_inst_cache_init(InstCache* cache) {
    if (!cache) {
        return false;
    }
    cache->slots = reinterpret_cast<InstCacheEntry*>(
        CPUEAXH_ALLOC_ZEROED(sizeof(InstCacheEntry) * CPU_INST_CACHE_SIZE));
    return cache->slots != NULL;
}

inline void cpu_inst_cache_destroy(InstCache* cache) {
    if (!cache) {
        return;
    }
    if (cache->slots) {
        CPUEAXH_FREE(cache->slots);
        cache->slots = NULL;
    }
}

inline void cpu_inst_cache_flush(InstCache* cache) {
    if (!cache || !cache->slots) {
        return;
    }
    CPUEAXH_MEMSET(cache->slots, 0, sizeof(InstCacheEntry) * CPU_INST_CACHE_SIZE);
}

// Look up an entry for the given RIP. Returns the slot pointer if it holds a
// valid hit (tag matches and the version snapshot matches the manager's
// current code_version). Returns NULL on a miss.
inline InstCacheEntry* cpu_inst_cache_lookup(
    InstCache* cache,
    const MEMORY_MANAGER* mgr,
    uint64_t rip,
    InstCacheModeKey mode)
{
    if (!cache || !cache->slots || !mgr) {
        return NULL;
    }
    InstCacheEntry* slot = &cache->slots[cpu_inst_cache_index(rip)];
    if (slot->tag != rip || slot->version != mgr->code_version || slot->mode.bits != mode.bits) {
#if defined(CPUEAXH_INST_CACHE_STATS)
        cache->misses++;
#endif
        return NULL;
    }
#if defined(CPUEAXH_INST_CACHE_STATS)
    cache->hits++;
#endif
    return slot;
}

// Reserve (and return) the slot that should hold the entry for `rip`.
// Caller is expected to populate `slot->decoded`, then call
// cpu_inst_cache_commit so we stamp tag + version atomically from the
// caller's perspective.
inline InstCacheEntry* cpu_inst_cache_reserve(InstCache* cache, uint64_t rip) {
    if (!cache || !cache->slots) {
        return NULL;
    }
    return &cache->slots[cpu_inst_cache_index(rip)];
}

inline void cpu_inst_cache_commit(
    InstCacheEntry* slot,
    const MEMORY_MANAGER* mgr,
    uint64_t rip,
    InstCacheModeKey mode)
{
    if (!slot || !mgr) {
        return;
    }
    slot->tag = rip;
    slot->version = mgr->code_version;
    slot->mode = mode;
}
