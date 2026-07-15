#include "core/memory/block_allocator.h"
#include "core/profiling/seed_assert.h"
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
    #include <malloc.h>
#else
    #include <sys/mman.h>
#endif

namespace seed::memory {

BlockAllocator::BlockAllocator(size_t blockSize) : m_blockSize(blockSize) {
}

BlockAllocator::~BlockAllocator() {
    for (auto& block : m_blocks) {
#ifdef _WIN32
        _aligned_free(block.base);
#else
        free(block.base);
#endif
    }
}

void* BlockAllocator::allocate(size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Simple bump allocator for demo
    if (m_blocks.empty() || m_blocks.back().used + size > m_blockSize) {
        Block newBlock;
#ifdef _WIN32
        newBlock.base = static_cast<uint8_t*>(_aligned_malloc(m_blockSize, alignment));
#else
        newBlock.base = static_cast<uint8_t*>(aligned_alloc(alignment, m_blockSize));
#endif
        SEED_ASSERT(newBlock.base != nullptr, "BlockAllocator: OS allocation failed");
        newBlock.used = 0;
        m_blocks.push_back(newBlock);
    }

    auto& block = m_blocks.back();
    size_t aligned = (block.used + alignment - 1) & ~(alignment - 1);
    void* ptr = block.base + aligned;
    block.used = aligned + size;
    m_allocated += size;

    return ptr;
}

void BlockAllocator::deallocate(void* ptr, size_t size) {
    // Block allocator doesn't truly free individual allocations
    (void)ptr;
    m_allocated -= size;
}

} // namespace seed::memory
