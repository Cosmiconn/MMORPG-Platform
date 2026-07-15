#include <doctest/doctest.h>
#include "core/memory/block_allocator.h"

using namespace seed::memory;

TEST_CASE("BlockAllocator_BasicAllocation") {
    BlockAllocator alloc;
    void* ptr = alloc.allocate(64, 8);
    CHECK(ptr != nullptr);
    alloc.deallocate(ptr, 64);
}
