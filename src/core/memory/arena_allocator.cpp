#include "core/memory/arena_allocator.h"
#include "core/profiling/seed_assert.h"
#include <cstdlib>

namespace seed::memory {

ArenaAllocator::ArenaAllocator(size_t arenaSize) : m_arenaSize(arenaSize) {
}

ArenaAllocator::~ArenaAllocator() {
    for (auto& arena : m_arenas) {
        free(arena.base);
    }
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    if (m_arenas.empty() || m_arenas.back().used + size > m_arenas.back().size) {
        Arena newArena;
        newArena.base = static_cast<uint8_t*>(malloc(m_arenaSize));
        SEED_ASSERT(newArena.base != nullptr, "ArenaAllocator: malloc failed");
        newArena.size = m_arenaSize;
        newArena.used = 0;
        m_arenas.push_back(newArena);
    }

    auto& arena = m_arenas.back();
    size_t aligned = (arena.used + alignment - 1) & ~(alignment - 1);
    void* ptr = arena.base + aligned;
    arena.used = aligned + size;
    m_totalUsed += size;

    return ptr;
}

void ArenaAllocator::deallocate(void* ptr, size_t size) {
    (void)ptr;
    m_totalUsed -= size;
}

void ArenaAllocator::reset() {
    for (auto& arena : m_arenas) {
        arena.used = 0;
    }
    m_totalUsed = 0;
}

size_t ArenaAllocator::totalUsed() const {
    return m_totalUsed;
}

} // namespace seed::memory
