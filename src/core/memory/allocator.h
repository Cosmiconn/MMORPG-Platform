#pragma once

#include <cstddef>
#include <cstdint>

namespace seed::memory {

class Allocator {
public:
    virtual ~Allocator() = default;
    virtual void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
    virtual void deallocate(void* ptr, size_t size = 0) = 0;
};

} // namespace seed::memory
