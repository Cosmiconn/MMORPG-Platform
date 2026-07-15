#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/arena_allocator.h"
#include "core/memory/stack_allocator.h"
#include "core/memory/memory_tracker.h"

namespace seed::memory {

class MemorySystem {
public:
    static MemorySystem& instance();

    void initialize();
    void shutdown();

    BlockAllocator* blockAllocator() { return &m_blockAlloc; }
    MemoryTracker* tracker() { return &m_tracker; }

private:
    BlockAllocator m_blockAlloc;
    MemoryTracker m_tracker;
};

} // namespace seed::memory
