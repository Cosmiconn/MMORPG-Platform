#include "core/memory/stack_allocator.h"

namespace seed::memory {

StackAllocator::StackAllocator(BlockAllocator* blockAlloc, size_t stackSize)
    : m_blockAlloc(blockAlloc)
    , m_capacity(stackSize)
    , m_used(0)
    , m_totalUsed(0)
{
    SEED_ZONE("StackAllocator::ctor");
    m_base = static_cast<uint8_t*>(m_blockAlloc->allocate(stackSize, 64));
}

StackAllocator::~StackAllocator() {
    SEED_ZONE("StackAllocator::dtor");
    // Stack memory returned when BlockAllocator is destroyed
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    SEED_ZONE("StackAllocator::allocate");

    size_t aligned = (m_used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > m_capacity) {
        return nullptr; // Stack overflow
    }

    void* ptr = m_base + aligned;
    m_used = aligned + size;
    m_totalUsed += size;
    return ptr;
}

void StackAllocator::deallocate(void* ptr, size_t size) {
    SEED_ZONE("StackAllocator::deallocate");
    (void)ptr;
    (void)size;
    // LIFO only: caller must use freeToMarker or reset
}

StackAllocator::Marker StackAllocator::getMarker() const {
    return {m_base + m_used, m_used};
}

void StackAllocator::freeToMarker(Marker marker) {
    SEED_ZONE("StackAllocator::freeToMarker");
    m_used = marker.used;
    m_totalUsed = marker.used; // approximate
}

void StackAllocator::reset() {
    SEED_ZONE("StackAllocator::reset");
    m_used = 0;
    m_totalUsed = 0;
}

} // namespace seed::memory
