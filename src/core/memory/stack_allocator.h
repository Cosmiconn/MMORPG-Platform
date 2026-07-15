#pragma once

#include "core/memory/allocator.h"
#include <vector>
#include <cstdint>

namespace seed::memory {

class StackAllocator : public Allocator {
public:
    explicit StackAllocator(size_t stackSize = 64 * 1024);
    ~StackAllocator() override;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size = 0) override;

    struct Marker {
        size_t arenaIndex;
        size_t offset;
    };

    Marker getMarker() const;
    void rollbackTo(Marker marker);

private:
    size_t m_stackSize;
    std::vector<uint8_t*> m_pages;
    size_t m_currentPage = 0;
    size_t m_offset = 0;
};

} // namespace seed::memory
