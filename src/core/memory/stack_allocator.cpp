#include "core/memory/stack_allocator.h"
#include "core/profiling/seed_assert.h"
#include <cstdlib>

namespace seed::memory {

StackAllocator::StackAllocator(size_t stackSize) : m_stackSize(stackSize) {
    uint8_t* page = static_cast<uint8_t*>(malloc(m_stackSize));
    SEED_ASSERT(page != nullptr, "StackAllocator: malloc failed");
    m_pages.push_back(page);
}

StackAllocator::~StackAllocator() {
    for (auto* page : m_pages) {
        free(page);
    }
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    size_t aligned = (m_offset + alignment - 1) & ~(alignment - 1);
    if (aligned + size > m_stackSize) {
        uint8_t* page = static_cast<uint8_t*>(malloc(m_stackSize));
        SEED_ASSERT(page != nullptr, "StackAllocator: malloc failed");
        m_pages.push_back(page);
        m_currentPage++;
        m_offset = 0;
        aligned = 0;
    }

    void* ptr = m_pages[m_currentPage] + aligned;
    m_offset = aligned + size;
    return ptr;
}

void StackAllocator::deallocate(void* ptr, size_t size) {
    (void)ptr;
    (void)size;
    // LIFO only - individual dealloc not supported
}

StackAllocator::Marker StackAllocator::getMarker() const {
    return {m_currentPage, m_offset};
}

void StackAllocator::rollbackTo(Marker marker) {
    m_currentPage = marker.arenaIndex;
    m_offset = marker.offset;
}

} // namespace seed::memory
