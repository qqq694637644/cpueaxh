#pragma once

#if defined(CPUEAXH_PLATFORM_KERNEL) || defined(_KERNEL_MODE) || defined(_NTDDK_) || defined(_WDM_INCLUDED_) || defined(_NTIFS_)

#include <ntddk.h>

#ifndef CPUEAXH_MEMSET
inline void* cpueaxh_platform_memset(void* dst, int value, size_t size) {
    RtlFillMemory(dst, size, (unsigned char)value);
    return dst;
}
#define CPUEAXH_MEMSET cpueaxh_platform_memset
#endif

#ifndef CPUEAXH_MEMCPY
inline void* cpueaxh_platform_memcpy(void* dst, const void* src, size_t size) {
    RtlCopyMemory(dst, src, size);
    return dst;
}
#define CPUEAXH_MEMCPY cpueaxh_platform_memcpy
#endif

#ifndef CPUEAXH_MEMMOVE
inline void* cpueaxh_platform_memmove(void* dst, const void* src, size_t size) {
    RtlMoveMemory(dst, src, size);
    return dst;
}
#define CPUEAXH_MEMMOVE cpueaxh_platform_memmove
#endif

#ifndef CPUEAXH_ALLOC_ZEROED
inline void* cpueaxh_platform_alloc_zeroed(size_t size) {
    if (size == 0) {
        return nullptr;
    }
#if defined(POOL_FLAG_NON_PAGED)
    void* block = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'haxE');
#elif defined(NonPagedPoolNx)
    void* block = ExAllocatePoolWithTag(NonPagedPoolNx, size, 'haxE');
#else
    void* block = ExAllocatePoolWithTag(NonPagedPool, size, 'haxE');
#endif
    if (!block) {
        return nullptr;
    }
    RtlZeroMemory(block, size);
    return block;
}
#define CPUEAXH_ALLOC_ZEROED(size) cpueaxh_platform_alloc_zeroed(size)
#endif

#ifndef CPUEAXH_FREE
inline void cpueaxh_platform_free(void* ptr) {
    if (ptr) {
        ExFreePoolWithTag(ptr, 'haxE');
    }
}
#define CPUEAXH_FREE(ptr) cpueaxh_platform_free(ptr)
#endif

inline uint64_t cpueaxh_udiv128_fallback(uint64_t high, uint64_t low, uint64_t divisor, uint64_t* remainder) {
    uint64_t quotient = 0;
    uint64_t rem = high;

    for (int bit = 63; bit >= 0; --bit) {
        rem = (rem << 1) | ((low >> bit) & 1ULL);
        if (rem >= divisor) {
            rem -= divisor;
            quotient |= (1ULL << bit);
        }
    }

    if (remainder) {
        *remainder = rem;
    }
    return quotient;
}

inline uint64_t cpueaxh_udiv128(uint64_t high, uint64_t low, uint64_t divisor, uint64_t* remainder) {
#if defined(_M_X64) && !defined(CPUEAXH_PLATFORM_KERNEL)
    return _udiv128(high, low, divisor, remainder);
#else
    return cpueaxh_udiv128_fallback(high, low, divisor, remainder);
#endif
}

#else

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <intrin.h>

#ifndef CPUEAXH_MEMSET
#define CPUEAXH_MEMSET memset
#endif

#ifndef CPUEAXH_MEMCPY
#define CPUEAXH_MEMCPY memcpy
#endif

#ifndef CPUEAXH_MEMMOVE
#define CPUEAXH_MEMMOVE memmove
#endif

#ifndef CPUEAXH_ALLOC_ZEROED
#define CPUEAXH_ALLOC_ZEROED(size) calloc(1, (size))
#endif

#ifndef CPUEAXH_FREE
#define CPUEAXH_FREE(ptr) free((ptr))
#endif

inline uint64_t cpueaxh_udiv128(uint64_t high, uint64_t low, uint64_t divisor, uint64_t* remainder) {
#if defined(_M_X64)
    return _udiv128(high, low, divisor, remainder);
#else
    uint64_t quotient = 0;
    uint64_t rem = high;

    for (int bit = 63; bit >= 0; --bit) {
        rem = (rem << 1) | ((low >> bit) & 1ULL);
        if (rem >= divisor) {
            rem -= divisor;
            quotient |= (1ULL << bit);
        }
    }

    if (remainder) {
        *remainder = rem;
    }
    return quotient;
#endif
}

#endif

#ifndef CPUEAXH_UNREACHABLE
#if defined(_MSC_VER) && (defined(CPUEAXH_PLATFORM_KERNEL) || defined(_KERNEL_MODE) || defined(_NTDDK_) || defined(_WDM_INCLUDED_) || defined(_NTIFS_))
#define CPUEAXH_UNREACHABLE() do { DbgBreakPoint(); __assume(0); } while (0)
#elif defined(_MSC_VER)
#define CPUEAXH_UNREACHABLE() do { __debugbreak(); __assume(0); } while (0)
#elif defined(__GNUC__) || defined(__clang__)
#define CPUEAXH_UNREACHABLE() do { __builtin_trap(); __builtin_unreachable(); } while (0)
#elif defined(CPUEAXH_PLATFORM_KERNEL) || defined(_KERNEL_MODE) || defined(_NTDDK_) || defined(_WDM_INCLUDED_) || defined(_NTIFS_)
#define CPUEAXH_UNREACHABLE() do { for (;;) { } } while (0)
#else
#define CPUEAXH_UNREACHABLE() abort()
#endif
#endif