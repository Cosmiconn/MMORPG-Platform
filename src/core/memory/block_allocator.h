#pragma once

#include "core/memory/allocator.h"
#include <vector>
#include <mutex>

namespace seed::memory {

class BlockAllocator : public Allocator {
public:
    BlockAllocator(size_t blockSize = 64 * 1024 * 1024); // 64MB default
    ~BlockAllocator() override;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size = 0) override;

    size_t allocatedBytes() const { return m_allocated; }
    size_t blockSize() const { return m_blockSize; }

private:
    struct Block {
        uint8_t* base = nullptr;
        size_t used = 0;
    };

    size_t m_blockSize;
    size_t m_allocated = 0;
    std::vector<Block> m_blocks;
    std::mutex m_mutex;
};

} // namespace seed::memory
