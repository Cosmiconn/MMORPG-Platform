#pragma once

#include "core/memory/block_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/arena_allocator.h"
#include "core/memory/stack_allocator.h"
#include "core/memory/memory_tracker.h"

namespace seed::memory {

// ---------------------------------------------------------------------------
// Global allocator instances (initialized in main, accessed everywhere)
// ---------------------------------------------------------------------------
extern BlockAllocator*  g_blockAllocator;
extern MemoryTracker*   g_memoryTracker;

// ---------------------------------------------------------------------------
// Scoped arena for frame-scoped allocations
// ---------------------------------------------------------------------------
extern ArenaAllocator*  g_frameArena;

// ---------------------------------------------------------------------------
// Tracy integration macros (memory profiling)
// ---------------------------------------------------------------------------
#if __has_include(<tracy/Tracy.hpp>)
#  include <tracy/Tracy.hpp>
#  define SEED_ZONE(name)           ZoneScopedN(name)
#  define SEED_ALLOC(ptr, size)     TracyAlloc(ptr, size)
#  define SEED_FREE(ptr)            TracyFree(ptr)
#  define SEED_FRAME_MARK()         FrameMark
#else
#  define SEED_ZONE(name)           ((void)sizeof(name))
#  define SEED_ALLOC(ptr, size)   ((void)sizeof(ptr), (void)sizeof(size))
#  define SEED_FREE(ptr)            ((void)sizeof(ptr))
#  define SEED_FRAME_MARK()         ((void)0)
#endif

// ---------------------------------------------------------------------------
// No-new/delete enforcement helpers
// ---------------------------------------------------------------------------
inline void* seed_alloc(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return g_blockAllocator->allocate(size, alignment);
}

inline void seed_free(void* ptr, size_t size = 0) {
    g_blockAllocator->deallocate(ptr, size);
}

} // namespace seed::memory
