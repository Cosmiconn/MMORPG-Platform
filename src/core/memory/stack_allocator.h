#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include <cstdint>
#include <vector>

// Tracy integration
#if __has_include(<tracy/Tracy.hpp>)
#  include <tracy/Tracy.hpp>
#  define SEED_ZONE(name) ZoneScopedN(name)
#else
#  define SEED_ZONE(name) ((void)sizeof(name))
#endif

namespace seed::memory {

// ---------------------------------------------------------------------------
// StackAllocator
// ---------------------------------------------------------------------------
// LIFO scope-based allocator.  Mark scope, allocate, free back to mark.
// NOT thread-safe.
// ---------------------------------------------------------------------------
class StackAllocator : public Allocator {
public:
    struct Marker {
        uint8_t* ptr;
        size_t used;
    };

    explicit StackAllocator(BlockAllocator* blockAlloc, size_t stackSize = 64 * 1024);
    ~StackAllocator() override;

    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    // Allocator interface
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void  deallocate(void* ptr, size_t size = 0) override;

    // Scope markers
    Marker getMarker() const;
    void freeToMarker(Marker marker);

    // Reset entire stack
    void reset();

    // Stats
    size_t totalUsed()     const { return m_totalUsed; }
    size_t totalCapacity() const { return m_capacity; }

private:
    BlockAllocator* m_blockAlloc;
    uint8_t* m_base;
    size_t   m_capacity;
    size_t   m_used;
    size_t   m_totalUsed;
};

} // namespace seed::memory
