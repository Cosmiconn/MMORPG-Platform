#pragma once

#include "core/memory/allocator.h"
#include <vector>

namespace seed::memory {

class ArenaAllocator : public Allocator {
public:
    explicit ArenaAllocator(size_t arenaSize = 64 * 1024);
    ~ArenaAllocator() override;

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    void reset();
    size_t totalUsed() const;

private:
    struct Arena {
        uint8_t* base = nullptr;
        size_t size = 0;
        size_t used = 0;
    };

    size_t m_arenaSize;
    std::vector<Arena> m_arenas;
    size_t m_totalUsed = 0;
};

} // namespace seed::memory
