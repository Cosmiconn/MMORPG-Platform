#include <doctest/doctest.h>
#include "core/memory/block_allocator.h"
#include "core/memory/arena_allocator.h"
#include "core/memory/stack_allocator.h"
#include "core/memory/memory_tracker.h"
#include "core/memory/pool_allocator.h"

using namespace seed::memory;

TEST_CASE("BlockAllocator_BasicAllocation") {
    BlockAllocator alloc(1024);
    void* p = alloc.allocate(100, 8);
    CHECK(p != nullptr);
    alloc.deallocate(p, 100);
}

TEST_CASE("ArenaAllocator_BasicAllocation") {
    ArenaAllocator alloc(1024);
    void* p = alloc.allocate(100, 8);
    CHECK(p != nullptr);
    alloc.reset();
}

TEST_CASE("StackAllocator_BasicLIFO") {
    StackAllocator alloc(1024);
    void* p1 = alloc.allocate(100, 8);
    void* p2 = alloc.allocate(100, 8);
    CHECK(p1 != nullptr);
    CHECK(p2 != nullptr);
    CHECK(p2 > p1);
}

TEST_CASE("MemoryTracker_BasicTracking") {
    MemoryTracker tracker;
    tracker.trackAllocation("test", 100);
    CHECK(tracker.totalAllocated() == 100);
    tracker.trackDeallocation("test", 100);
    CHECK(tracker.totalAllocated() == 100); // totalAllocated is cumulative
}

TEST_CASE("PoolAllocator_SingleThread") {
    BlockAllocator block;
    PoolAllocator<int> pool(&block);

    int* p1 = pool.allocate();
    *p1 = 42;
    CHECK(*p1 == 42);

    pool.deallocate(p1);
}
