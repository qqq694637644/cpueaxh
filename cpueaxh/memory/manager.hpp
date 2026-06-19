#pragma once

// memory/manager.hpp - Virtual memory manager with page-granularity allocation

#pragma once

#include "../cpueaxh_platform.hpp"

#define CPUEAXH_PAGE_SIZE 0x1000ULL
#define CPUEAXH_PAGE_MASK (~(CPUEAXH_PAGE_SIZE - 1))
#define MM_PAGE_CACHE_SIZE 256u

#define MM_PROT_READ  0x1u
#define MM_PROT_WRITE 0x2u
#define MM_PROT_EXEC  0x4u

#define MM_CPU_ATTR_USER 0x1u

enum MM_ACCESS_STATUS : uint32_t {
    MM_ACCESS_OK = 0,
    MM_ACCESS_UNMAPPED = 1,
    MM_ACCESS_PROT = 2,
};

struct MEMORY_REGION {
    uint64_t base;
    uint64_t size;
    uint8_t* data;
    uint32_t perms;
    uint32_t cpu_attrs;
    bool external;
};

struct MM_PAGE_CACHE_ENTRY {
    uint64_t page_base;
    uint8_t* host_page;
    uint32_t perms;
    uint32_t cpu_attrs;
    bool valid;
};

struct MM_PATCH_ENTRY {
    uint64_t handle;
    uint64_t address;
    uint64_t size;
    uint8_t* data;
};

enum MM_PATCH_STATUS : uint32_t {
    MM_PATCH_OK = 0,
    MM_PATCH_ARG = 1,
    MM_PATCH_NOMEM = 2,
    MM_PATCH_CONFLICT = 3,
    MM_PATCH_NOT_FOUND = 4,
};

struct MEMORY_MANAGER {
    MEMORY_REGION* regions;
    size_t region_count;
    size_t region_capacity;
    MM_PATCH_ENTRY* patches;
    size_t patch_count;
    size_t patch_capacity;
    uint64_t patch_min_address;
    uint64_t patch_max_address;
    uint64_t next_patch_handle;
    bool host_read_passthrough;
    bool host_write_passthrough;
    bool host_exec_passthrough;
    MM_PAGE_CACHE_ENTRY page_cache[MM_PAGE_CACHE_SIZE];
    // Monotonic counter bumped whenever the executable view of guest memory
    // changes (mappings, protections, host passthrough toggles, code patches).
    // The instruction cache uses it to invalidate stale decoded entries.
    uint64_t code_version;
};

struct MM_ACCESS_INFO {
    uint8_t* ptr;
    uint32_t perms;
    uint32_t cpu_attrs;
    bool mapped;
};

inline bool mm_patch_range_overlaps(uint64_t left_address, uint64_t left_size, uint64_t right_address, uint64_t right_size);
inline const MM_PATCH_ENTRY* mm_find_patch_const(const MEMORY_MANAGER* mgr, uint64_t address);
inline bool mm_query(MEMORY_MANAGER* mgr, uint64_t address, MM_ACCESS_INFO* out_info);

inline bool mm_patch_bounds_may_contain(const MEMORY_MANAGER* mgr, uint64_t address) {
    return mgr && mgr->patch_count != 0 && address >= mgr->patch_min_address && address <= mgr->patch_max_address;
}

inline void mm_rebuild_patch_bounds(MEMORY_MANAGER* mgr) {
    if (!mgr || mgr->patch_count == 0) {
        if (mgr) {
            mgr->patch_min_address = 0;
            mgr->patch_max_address = 0;
        }
        return;
    }

    uint64_t min_address = (uint64_t)-1;
    uint64_t max_address = 0;
    for (size_t index = 0; index < mgr->patch_count; ++index) {
        const MM_PATCH_ENTRY* patch = &mgr->patches[index];
        if (patch->size == 0) {
            continue;
        }

        if (patch->address < min_address) {
            min_address = patch->address;
        }

        const uint64_t patch_end = patch->address + patch->size - 1;
        if (patch_end > max_address) {
            max_address = patch_end;
        }
    }

    mgr->patch_min_address = min_address == (uint64_t)-1 ? 0 : min_address;
    mgr->patch_max_address = max_address;
}

inline uint64_t align_up_page(uint64_t size) {
    return (size + CPUEAXH_PAGE_SIZE - 1) & CPUEAXH_PAGE_MASK;
}

inline uint64_t align_down_page(uint64_t addr) {
    return addr & CPUEAXH_PAGE_MASK;
}

inline bool mm_is_page_aligned(uint64_t value) {
    return (value & (CPUEAXH_PAGE_SIZE - 1)) == 0;
}

inline bool mm_is_valid_perms(uint32_t perms) {
    return (perms & ~(MM_PROT_READ | MM_PROT_WRITE | MM_PROT_EXEC)) == 0;
}

inline bool mm_is_valid_cpu_attrs(uint32_t attrs) {
    return (attrs & ~MM_CPU_ATTR_USER) == 0;
}

inline bool mm_range_overflows(uint64_t address, uint64_t size) {
    return size != 0 && (address + size - 1) < address;
}

inline void mm_init(MEMORY_MANAGER* mgr) {
    CPUEAXH_MEMSET(mgr, 0, sizeof(MEMORY_MANAGER));
    mgr->next_patch_handle = 1;
    mgr->code_version = 1;
}

inline void mm_bump_code_version(MEMORY_MANAGER* mgr) {
    if (!mgr) {
        return;
    }
    mgr->code_version++;
    if (mgr->code_version == 0) {
        mgr->code_version = 1;
    }
}

inline void mm_invalidate_cache(MEMORY_MANAGER* mgr) {
    if (!mgr) {
        return;
    }
    CPUEAXH_MEMSET(mgr->page_cache, 0, sizeof(mgr->page_cache));
    mm_bump_code_version(mgr);
}

inline uint32_t mm_host_passthrough_perms(const MEMORY_MANAGER* mgr) {
    uint32_t perms = 0;
    if (!mgr) {
        return 0;
    }
    if (mgr->host_read_passthrough) {
        perms |= MM_PROT_READ;
    }
    if (mgr->host_write_passthrough) {
        perms |= MM_PROT_WRITE;
    }
    if (mgr->host_exec_passthrough) {
        perms |= MM_PROT_EXEC;
    }
    return perms;
}

inline bool mm_has_host_passthrough(const MEMORY_MANAGER* mgr, uint32_t perm) {
    if (!mgr) {
        return false;
    }

    switch (perm) {
    case MM_PROT_READ:
        return mgr->host_read_passthrough;
    case MM_PROT_WRITE:
        return mgr->host_write_passthrough;
    case MM_PROT_EXEC:
        return mgr->host_exec_passthrough;
    default:
        return false;
    }
}

inline void mm_set_host_passthrough(MEMORY_MANAGER* mgr, uint32_t perms, bool enabled) {
    mgr->host_read_passthrough = enabled && ((perms & MM_PROT_READ) != 0);
    mgr->host_write_passthrough = enabled && ((perms & MM_PROT_WRITE) != 0);
    mgr->host_exec_passthrough = enabled && ((perms & MM_PROT_EXEC) != 0);
    mm_invalidate_cache(mgr);
}

inline void mm_release_region(MEMORY_REGION* region) {
    if (!region) {
        return;
    }
    if (!region->external && region->data) {
        CPUEAXH_FREE(region->data);
    }
    CPUEAXH_MEMSET(region, 0, sizeof(*region));
}

inline bool mm_reserve_region_capacity(MEMORY_MANAGER* mgr, size_t capacity) {
    if (capacity <= mgr->region_capacity) {
        return true;
    }

    size_t new_capacity = mgr->region_capacity == 0 ? 16 : mgr->region_capacity;
    while (new_capacity < capacity) {
        if (new_capacity > ((size_t)-1) / 2) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }

    MEMORY_REGION* new_regions = reinterpret_cast<MEMORY_REGION*>(
        CPUEAXH_ALLOC_ZEROED(new_capacity * sizeof(MEMORY_REGION)));
    if (!new_regions) {
        return false;
    }

    if (mgr->regions && mgr->region_count != 0) {
        CPUEAXH_MEMCPY(new_regions, mgr->regions, mgr->region_count * sizeof(MEMORY_REGION));
        CPUEAXH_FREE(mgr->regions);
    }

    mgr->regions = new_regions;
    mgr->region_capacity = new_capacity;
    return true;
}

inline bool mm_patch_range_overlaps(uint64_t left_address, uint64_t left_size, uint64_t right_address, uint64_t right_size) {
    if (left_size == 0 || right_size == 0) {
        return false;
    }

    const uint64_t left_end = left_address + left_size - 1;
    const uint64_t right_end = right_address + right_size - 1;
    return !(left_end < right_address || right_end < left_address);
}

inline bool mm_reserve_patch_capacity(MEMORY_MANAGER* mgr, size_t capacity) {
    if (capacity <= mgr->patch_capacity) {
        return true;
    }

    size_t new_capacity = mgr->patch_capacity == 0 ? 8 : mgr->patch_capacity;
    while (new_capacity < capacity) {
        if (new_capacity > ((size_t)-1) / 2) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }

    MM_PATCH_ENTRY* new_patches = reinterpret_cast<MM_PATCH_ENTRY*>(
        CPUEAXH_ALLOC_ZEROED(new_capacity * sizeof(MM_PATCH_ENTRY)));
    if (!new_patches) {
        return false;
    }

    if (mgr->patches && mgr->patch_count != 0) {
        CPUEAXH_MEMCPY(new_patches, mgr->patches, mgr->patch_count * sizeof(MM_PATCH_ENTRY));
        CPUEAXH_FREE(mgr->patches);
    }

    mgr->patches = new_patches;
    mgr->patch_capacity = new_capacity;
    return true;
}

inline MM_PATCH_ENTRY* mm_find_patch(MEMORY_MANAGER* mgr, uint64_t address) {
    if (!mm_patch_bounds_may_contain(mgr, address)) {
        return NULL;
    }

    for (size_t index = 0; index < mgr->patch_count; ++index) {
        MM_PATCH_ENTRY* patch = &mgr->patches[index];
        if (patch->size == 0) {
            continue;
        }

        const uint64_t patch_end = patch->address + patch->size - 1;
        if (address >= patch->address && address <= patch_end) {
            return patch;
        }
    }

    return NULL;
}

inline const MM_PATCH_ENTRY* mm_find_patch_const(const MEMORY_MANAGER* mgr, uint64_t address) {
    return mm_find_patch(const_cast<MEMORY_MANAGER*>(mgr), address);
}

inline MM_PATCH_STATUS mm_add_patch(MEMORY_MANAGER* mgr, uint64_t* out_handle, uint64_t address, const void* bytes, uint64_t size) {
    if (!mgr || !out_handle || !bytes || size == 0 || mm_range_overflows(address, size)) {
        return MM_PATCH_ARG;
    }

    for (size_t index = 0; index < mgr->patch_count; ++index) {
        const MM_PATCH_ENTRY* patch = &mgr->patches[index];
        if (mm_patch_range_overlaps(address, size, patch->address, patch->size)) {
            return MM_PATCH_CONFLICT;
        }
    }

    if (!mm_reserve_patch_capacity(mgr, mgr->patch_count + 1)) {
        return MM_PATCH_NOMEM;
    }

    uint8_t* patch_bytes = reinterpret_cast<uint8_t*>(CPUEAXH_ALLOC_ZEROED((size_t)size));
    if (!patch_bytes) {
        return MM_PATCH_NOMEM;
    }

    CPUEAXH_MEMCPY(patch_bytes, bytes, (size_t)size);

    MM_PATCH_ENTRY* patch = &mgr->patches[mgr->patch_count++];
    patch->handle = mgr->next_patch_handle++;
    patch->address = address;
    patch->size = size;
    patch->data = patch_bytes;
    const uint64_t patch_end = address + size - 1;
    if (mgr->patch_count == 1 || address < mgr->patch_min_address) {
        mgr->patch_min_address = address;
    }
    if (mgr->patch_count == 1 || patch_end > mgr->patch_max_address) {
        mgr->patch_max_address = patch_end;
    }
    *out_handle = patch->handle;
    mm_bump_code_version(mgr);
    return MM_PATCH_OK;
}

inline MM_PATCH_STATUS mm_del_patch(MEMORY_MANAGER* mgr, uint64_t handle) {
    if (!mgr || handle == 0) {
        return MM_PATCH_ARG;
    }

    for (size_t index = 0; index < mgr->patch_count; ++index) {
        MM_PATCH_ENTRY* patch = &mgr->patches[index];
        if (patch->handle != handle) {
            continue;
        }

        if (patch->data) {
            CPUEAXH_FREE(patch->data);
        }

        if (index + 1 < mgr->patch_count) {
            CPUEAXH_MEMMOVE(
                &mgr->patches[index],
                &mgr->patches[index + 1],
                (mgr->patch_count - index - 1) * sizeof(MM_PATCH_ENTRY));
        }

        mgr->patch_count--;
        if (mgr->patch_count < mgr->patch_capacity) {
            CPUEAXH_MEMSET(&mgr->patches[mgr->patch_count], 0, sizeof(MM_PATCH_ENTRY));
        }
        mm_rebuild_patch_bounds(mgr);
        mm_bump_code_version(mgr);
        return MM_PATCH_OK;
    }

    return MM_PATCH_NOT_FOUND;
}

inline size_t mm_find_insertion_index(const MEMORY_MANAGER* mgr, uint64_t base) {
    size_t left = 0;
    size_t right = mgr->region_count;
    while (left < right) {
        size_t mid = left + ((right - left) / 2);
        if (mgr->regions[mid].base < base) {
            left = mid + 1;
        }
        else {
            right = mid;
        }
    }
    return left;
}

inline size_t mm_find_region_index(const MEMORY_MANAGER* mgr, uint64_t address) {
    size_t left = 0;
    size_t right = mgr->region_count;

    while (left < right) {
        size_t mid = left + ((right - left) / 2);
        const MEMORY_REGION* region = &mgr->regions[mid];
        const uint64_t region_end = region->base + region->size;
        if (address < region->base) {
            right = mid;
        }
        else if (address >= region_end) {
            left = mid + 1;
        }
        else {
            return mid;
        }
    }

    return (size_t)-1;
}

inline MEMORY_REGION* mm_find_region(MEMORY_MANAGER* mgr, uint64_t address) {
    size_t index = mm_find_region_index(mgr, address);
    return index == (size_t)-1 ? NULL : &mgr->regions[index];
}

inline const MEMORY_REGION* mm_find_region_const(const MEMORY_MANAGER* mgr, uint64_t address) {
    size_t index = mm_find_region_index(mgr, address);
    return index == (size_t)-1 ? NULL : &mgr->regions[index];
}

inline bool mm_has_overlap(MEMORY_MANAGER* mgr, uint64_t base, uint64_t size) {
    if (size == 0 || mm_range_overflows(base, size)) {
        return true;
    }

    const uint64_t end = base + size;
    size_t index = mm_find_insertion_index(mgr, base);

    if (index > 0) {
        const MEMORY_REGION* previous = &mgr->regions[index - 1];
        if ((previous->base + previous->size) > base) {
            return true;
        }
    }

    if (index < mgr->region_count) {
        const MEMORY_REGION* current = &mgr->regions[index];
        if (current->base < end) {
            return true;
        }
    }

    return false;
}

inline bool mm_insert_region(MEMORY_MANAGER* mgr, size_t index, const MEMORY_REGION* region) {
    if (!mm_reserve_region_capacity(mgr, mgr->region_count + 1)) {
        return false;
    }

    if (index < mgr->region_count) {
        CPUEAXH_MEMMOVE(
            &mgr->regions[index + 1],
            &mgr->regions[index],
            (mgr->region_count - index) * sizeof(MEMORY_REGION));
    }

    mgr->regions[index] = *region;
    mgr->region_count++;
    return true;
}

inline bool mm_replace_region(MEMORY_MANAGER* mgr, size_t index, const MEMORY_REGION* replacements, size_t replacement_count) {
    if (index >= mgr->region_count) {
        return false;
    }

    const size_t new_count = mgr->region_count - 1 + replacement_count;
    if (!mm_reserve_region_capacity(mgr, new_count)) {
        return false;
    }

    MEMORY_REGION original = mgr->regions[index];
    const size_t tail_count = mgr->region_count - index - 1;

    if (replacement_count > 1) {
        CPUEAXH_MEMMOVE(
            &mgr->regions[index + replacement_count],
            &mgr->regions[index + 1],
            tail_count * sizeof(MEMORY_REGION));
    }
    else if (replacement_count == 0 && tail_count != 0) {
        CPUEAXH_MEMMOVE(
            &mgr->regions[index],
            &mgr->regions[index + 1],
            tail_count * sizeof(MEMORY_REGION));
    }

    for (size_t i = 0; i < replacement_count; i++) {
        mgr->regions[index + i] = replacements[i];
    }

    mgr->region_count = new_count;
    mm_release_region(&original);
    mm_invalidate_cache(mgr);
    return true;
}

inline bool mm_check_range_mapped(const MEMORY_MANAGER* mgr, uint64_t address, uint64_t size) {
    if (size == 0) {
        return true;
    }
    if (mm_range_overflows(address, size)) {
        return false;
    }

    const uint64_t end = address + size;
    uint64_t current = address;

    while (current < end) {
        size_t index = mm_find_region_index(mgr, current);
        if (index == (size_t)-1) {
            return false;
        }

        const MEMORY_REGION* region = &mgr->regions[index];
        const uint64_t region_end = region->base + region->size;
        if (region_end <= current) {
            return false;
        }

        current = region_end < end ? region_end : end;
    }

    return true;
}

inline bool mm_build_region_segment(
    const MEMORY_REGION* source,
    uint64_t segment_base,
    uint64_t segment_size,
    uint32_t perms,
    uint32_t cpu_attrs,
    MEMORY_REGION* out_region) {
    if (!source || !out_region || segment_size == 0) {
        return false;
    }

    CPUEAXH_MEMSET(out_region, 0, sizeof(*out_region));
    out_region->base = segment_base;
    out_region->size = segment_size;
    out_region->perms = perms;
    out_region->cpu_attrs = cpu_attrs;
    out_region->external = source->external;

    const uint64_t offset = segment_base - source->base;
    if (source->external) {
        out_region->data = source->data + offset;
        return true;
    }

    out_region->data = reinterpret_cast<uint8_t*>(CPUEAXH_ALLOC_ZEROED((size_t)segment_size));
    if (!out_region->data) {
        return false;
    }

    CPUEAXH_MEMCPY(out_region->data, source->data + offset, (size_t)segment_size);
    return true;
}

inline bool mm_map_internal(MEMORY_MANAGER* mgr, uint64_t address, uint64_t size, uint32_t perms) {
    if (size == 0 || !mm_is_page_aligned(address) || !mm_is_page_aligned(size) || mm_range_overflows(address, size)) {
        return false;
    }
    if (!mm_is_valid_perms(perms) || mm_has_overlap(mgr, address, size)) {
        return false;
    }

    MEMORY_REGION region = {};
    region.base = address;
    region.size = size;
    region.perms = perms;
    region.cpu_attrs = MM_CPU_ATTR_USER;
    region.external = false;
    region.data = reinterpret_cast<uint8_t*>(CPUEAXH_ALLOC_ZEROED((size_t)size));
    if (!region.data) {
        return false;
    }

    bool inserted = mm_insert_region(mgr, mm_find_insertion_index(mgr, address), &region);
    if (!inserted) {
        mm_release_region(&region);
        return false;
    }

    mm_invalidate_cache(mgr);
    return true;
}

inline bool mm_map_host(MEMORY_MANAGER* mgr, uint64_t address, uint64_t size, uint32_t perms, void* host_ptr) {
    if (size == 0 || host_ptr == NULL || !mm_is_page_aligned(address) || !mm_is_page_aligned(size) || mm_range_overflows(address, size)) {
        return false;
    }
    if (!mm_is_valid_perms(perms) || mm_has_overlap(mgr, address, size)) {
        return false;
    }

    MEMORY_REGION region = {};
    region.base = address;
    region.size = size;
    region.data = reinterpret_cast<uint8_t*>(host_ptr);
    region.perms = perms;
    region.cpu_attrs = MM_CPU_ATTR_USER;
    region.external = true;

    bool inserted = mm_insert_region(mgr, mm_find_insertion_index(mgr, address), &region);
    if (!inserted) {
        return false;
    }

    mm_invalidate_cache(mgr);
    return true;
}

inline bool mm_alloc(MEMORY_MANAGER* mgr, uint64_t address, uint64_t size) {
    return mm_map_internal(mgr, address, size, MM_PROT_READ | MM_PROT_WRITE | MM_PROT_EXEC);
}

inline bool mm_unmap(MEMORY_MANAGER* mgr, uint64_t address, uint64_t size) {
    if (size == 0) {
        return true;
    }
    if (!mm_is_page_aligned(address) || !mm_is_page_aligned(size) || mm_range_overflows(address, size)) {
        return false;
    }
    if (!mm_check_range_mapped(mgr, address, size)) {
        return false;
    }

    const uint64_t end = address + size;
    uint64_t current = address;

    while (current < end) {
        size_t index = mm_find_region_index(mgr, current);
        if (index == (size_t)-1) {
            return false;
        }

        const MEMORY_REGION original = mgr->regions[index];
        const uint64_t original_end = original.base + original.size;
        const uint64_t chunk_end = original_end < end ? original_end : end;

        MEMORY_REGION replacements[2] = {};
        size_t replacement_count = 0;

        if (original.base < current) {
            if (!mm_build_region_segment(&original, original.base, current - original.base, original.perms, original.cpu_attrs, &replacements[replacement_count++])) {
                for (size_t i = 0; i < replacement_count; i++) {
                    mm_release_region(&replacements[i]);
                }
                return false;
            }
        }

        if (chunk_end < original_end) {
            if (!mm_build_region_segment(&original, chunk_end, original_end - chunk_end, original.perms, original.cpu_attrs, &replacements[replacement_count++])) {
                for (size_t i = 0; i < replacement_count; i++) {
                    mm_release_region(&replacements[i]);
                }
                return false;
            }
        }

        if (!mm_replace_region(mgr, index, replacements, replacement_count)) {
            for (size_t i = 0; i < replacement_count; i++) {
                mm_release_region(&replacements[i]);
            }
            return false;
        }

        current = chunk_end;
    }

    return true;
}

inline bool mm_protect(MEMORY_MANAGER* mgr, uint64_t address, uint64_t size, uint32_t perms) {
    if (size == 0) {
        return true;
    }
    if (!mm_is_page_aligned(address) || !mm_is_page_aligned(size) || mm_range_overflows(address, size)) {
        return false;
    }
    if (!mm_is_valid_perms(perms) || !mm_check_range_mapped(mgr, address, size)) {
        return false;
    }

    const uint64_t end = address + size;
    uint64_t current = address;

    while (current < end) {
        size_t index = mm_find_region_index(mgr, current);
        if (index == (size_t)-1) {
            return false;
        }

        const MEMORY_REGION original = mgr->regions[index];
        const uint64_t original_end = original.base + original.size;
        const uint64_t chunk_end = original_end < end ? original_end : end;

        MEMORY_REGION replacements[3] = {};
        size_t replacement_count = 0;

        if (original.base < current) {
            if (!mm_build_region_segment(&original, original.base, current - original.base, original.perms, original.cpu_attrs, &replacements[replacement_count++])) {
                for (size_t i = 0; i < replacement_count; i++) {
                    mm_release_region(&replacements[i]);
                }
                return false;
            }
        }

        if (!mm_build_region_segment(&original, current, chunk_end - current, perms, original.cpu_attrs, &replacements[replacement_count++])) {
            for (size_t i = 0; i < replacement_count; i++) {
                mm_release_region(&replacements[i]);
            }
            return false;
        }

        if (chunk_end < original_end) {
            if (!mm_build_region_segment(&original, chunk_end, original_end - chunk_end, original.perms, original.cpu_attrs, &replacements[replacement_count++])) {
                for (size_t i = 0; i < replacement_count; i++) {
                    mm_release_region(&replacements[i]);
                }
                return false;
            }
        }

        if (!mm_replace_region(mgr, index, replacements, replacement_count)) {
            for (size_t i = 0; i < replacement_count; i++) {
                mm_release_region(&replacements[i]);
            }
            return false;
        }

        current = chunk_end;
    }

    return true;
}

inline bool mm_set_cpu_attrs(MEMORY_MANAGER* mgr, uint64_t address, uint64_t size, uint32_t cpu_attrs) {
    if (size == 0) {
        return true;
    }
    if (!mm_is_page_aligned(address) || !mm_is_page_aligned(size) || mm_range_overflows(address, size)) {
        return false;
    }
    if (!mm_is_valid_cpu_attrs(cpu_attrs) || !mm_check_range_mapped(mgr, address, size)) {
        return false;
    }

    const uint64_t end = address + size;
    uint64_t current = address;

    while (current < end) {
        size_t index = mm_find_region_index(mgr, current);
        if (index == (size_t)-1) {
            return false;
        }

        const MEMORY_REGION original = mgr->regions[index];
        const uint64_t original_end = original.base + original.size;
        const uint64_t chunk_end = original_end < end ? original_end : end;

        MEMORY_REGION replacements[3] = {};
        size_t replacement_count = 0;

        if (original.base < current) {
            if (!mm_build_region_segment(&original, original.base, current - original.base, original.perms, original.cpu_attrs, &replacements[replacement_count++])) {
                for (size_t i = 0; i < replacement_count; i++) {
                    mm_release_region(&replacements[i]);
                }
                return false;
            }
        }

        if (!mm_build_region_segment(&original, current, chunk_end - current, original.perms, cpu_attrs, &replacements[replacement_count++])) {
            for (size_t i = 0; i < replacement_count; i++) {
                mm_release_region(&replacements[i]);
            }
            return false;
        }

        if (chunk_end < original_end) {
            if (!mm_build_region_segment(&original, chunk_end, original_end - chunk_end, original.perms, original.cpu_attrs, &replacements[replacement_count++])) {
                for (size_t i = 0; i < replacement_count; i++) {
                    mm_release_region(&replacements[i]);
                }
                return false;
            }
        }

        if (!mm_replace_region(mgr, index, replacements, replacement_count)) {
            for (size_t i = 0; i < replacement_count; i++) {
                mm_release_region(&replacements[i]);
            }
            return false;
        }

        current = chunk_end;
    }

    return true;
}

inline size_t mm_cache_slot(uint64_t address) {
    return (size_t)((address >> 12) & (MM_PAGE_CACHE_SIZE - 1));
}

inline bool mm_query(MEMORY_MANAGER* mgr, uint64_t address, MM_ACCESS_INFO* out_info) {
    if (!mgr || !out_info) {
        return false;
    }

    const uint64_t page_base = align_down_page(address);
    const size_t slot = mm_cache_slot(address);
    MM_PAGE_CACHE_ENTRY* cache_entry = &mgr->page_cache[slot];
    if (cache_entry->valid && cache_entry->page_base == page_base) {
        out_info->ptr = cache_entry->host_page + (size_t)(address - page_base);
        out_info->perms = cache_entry->perms;
        out_info->cpu_attrs = cache_entry->cpu_attrs;
        out_info->mapped = true;
        return true;
    }

    size_t index = mm_find_region_index(mgr, address);
    if (index != (size_t)-1) {
        MEMORY_REGION* region = &mgr->regions[index];
        cache_entry->valid = true;
        cache_entry->page_base = page_base;
        cache_entry->host_page = region->data + (page_base - region->base);
        cache_entry->perms = region->perms;
        cache_entry->cpu_attrs = region->cpu_attrs;

        out_info->ptr = cache_entry->host_page + (size_t)(address - page_base);
        out_info->perms = cache_entry->perms;
        out_info->cpu_attrs = cache_entry->cpu_attrs;
        out_info->mapped = true;
        return true;
    }

    const uint32_t host_perms = mm_host_passthrough_perms(mgr);
    if (host_perms != 0) {
        cache_entry->valid = true;
        cache_entry->page_base = page_base;
        cache_entry->host_page = reinterpret_cast<uint8_t*>((uintptr_t)page_base);
        cache_entry->perms = host_perms;
        cache_entry->cpu_attrs = MM_CPU_ATTR_USER;

        out_info->ptr = cache_entry->host_page + (size_t)(address - page_base);
        out_info->perms = cache_entry->perms;
        out_info->cpu_attrs = cache_entry->cpu_attrs;
        out_info->mapped = true;
        return true;
    }

    out_info->ptr = NULL;
    out_info->perms = 0;
    out_info->cpu_attrs = 0;
    out_info->mapped = false;
    return true;
}

inline MM_ACCESS_STATUS mm_get_ptr_checked(MEMORY_MANAGER* mgr, uint64_t address, uint32_t perm, uint8_t** out_ptr, uint32_t* out_cpu_attrs = NULL) {
    if (!out_ptr) {
        return MM_ACCESS_UNMAPPED;
    }

    MM_ACCESS_INFO info = {};
    if (!mm_query(mgr, address, &info) || !info.mapped) {
        *out_ptr = NULL;
        if (out_cpu_attrs) {
            *out_cpu_attrs = 0;
        }
        return MM_ACCESS_UNMAPPED;
    }

    if ((info.perms & perm) == 0) {
        *out_ptr = NULL;
        if (out_cpu_attrs) {
            *out_cpu_attrs = info.cpu_attrs;
        }
        return MM_ACCESS_PROT;
    }

    const MM_PATCH_ENTRY* patch = NULL;
    if (mgr->patch_count != 0 && mm_host_passthrough_perms(mgr) != 0 && mm_patch_bounds_may_contain(mgr, address)) {
        patch = mm_find_patch_const(mgr, address);
    }

    if (patch) {
        *out_ptr = patch->data + (size_t)(address - patch->address);
    }
    else {
        *out_ptr = info.ptr;
    }

    if (out_cpu_attrs) {
        *out_cpu_attrs = info.cpu_attrs;
    }
    return MM_ACCESS_OK;
}

inline MM_ACCESS_STATUS mm_read_byte_checked(MEMORY_MANAGER* mgr, uint64_t address, uint8_t* out, uint32_t perm, uint32_t* out_cpu_attrs = NULL) {
    if (!out) {
        return MM_ACCESS_UNMAPPED;
    }

    uint8_t* ptr = NULL;
    MM_ACCESS_STATUS status = mm_get_ptr_checked(mgr, address, perm, &ptr, out_cpu_attrs);
    if (status != MM_ACCESS_OK) {
        return status;
    }

    *out = *ptr;
    return MM_ACCESS_OK;
}

inline MM_ACCESS_STATUS mm_write_byte_checked(MEMORY_MANAGER* mgr, uint64_t address, uint8_t value, uint32_t* out_cpu_attrs = NULL) {
    uint8_t* ptr = NULL;
    MM_ACCESS_STATUS status = mm_get_ptr_checked(mgr, address, MM_PROT_WRITE, &ptr, out_cpu_attrs);
    if (status != MM_ACCESS_OK) {
        return status;
    }

    *ptr = value;
    return MM_ACCESS_OK;
}

inline uint8_t* mm_translate(MEMORY_MANAGER* mgr, uint64_t address) {
    MM_ACCESS_INFO info = {};
    if (!mm_query(mgr, address, &info) || !info.mapped) {
        return NULL;
    }
    return info.ptr;
}

inline const uint8_t* mm_translate_const(const MEMORY_MANAGER* mgr, uint64_t address) {
    return mm_translate(const_cast<MEMORY_MANAGER*>(mgr), address);
}

inline bool mm_read_byte_with_perm(MEMORY_MANAGER* mgr, uint64_t address, uint8_t* out, uint32_t perm) {
    return mm_read_byte_checked(mgr, address, out, perm) == MM_ACCESS_OK;
}

inline bool mm_read_byte(MEMORY_MANAGER* mgr, uint64_t address, uint8_t* out) {
    return mm_read_byte_with_perm(mgr, address, out, MM_PROT_READ);
}

inline bool mm_read_exec_byte(MEMORY_MANAGER* mgr, uint64_t address, uint8_t* out) {
    return mm_read_byte_with_perm(mgr, address, out, MM_PROT_EXEC);
}

inline bool mm_write_byte(MEMORY_MANAGER* mgr, uint64_t address, uint8_t value) {
    return mm_write_byte_checked(mgr, address, value) == MM_ACCESS_OK;
}

inline uint8_t* mm_get_ptr_with_perm(MEMORY_MANAGER* mgr, uint64_t address, uint32_t perm) {
    uint8_t* ptr = NULL;
    return mm_get_ptr_checked(mgr, address, perm, &ptr) == MM_ACCESS_OK ? ptr : NULL;
}

inline void mm_destroy(MEMORY_MANAGER* mgr) {
    if (!mgr) {
        return;
    }
    for (size_t i = 0; i < mgr->region_count; i++) {
        mm_release_region(&mgr->regions[i]);
    }
    for (size_t i = 0; i < mgr->patch_count; i++) {
        if (mgr->patches[i].data) {
            CPUEAXH_FREE(mgr->patches[i].data);
        }
    }
    if (mgr->regions) {
        CPUEAXH_FREE(mgr->regions);
    }
    if (mgr->patches) {
        CPUEAXH_FREE(mgr->patches);
    }
    CPUEAXH_MEMSET(mgr, 0, sizeof(MEMORY_MANAGER));
}
