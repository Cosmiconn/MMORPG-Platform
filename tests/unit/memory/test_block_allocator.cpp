#include <doctest/doctest.h>
#include "core/memory/block_allocator.h"

#include <thread>
#include <vector>

using namespace seed::memory;

// ---------------------------------------------------------------------------
// Basic functionality
// ---------------------------------------------------------------------------
TEST_CASE("BlockAllocator_BasicAllocation") {
    BlockAllocator alloc;

    void* p = alloc.allocate(1024);
    CHECK(p != nullptr);
    CHECK(alloc.numBlocks() == 1);
    CHECK(alloc.totalAllocated() >= 1024);
    CHECK(alloc.totalUsed() >= 1024);

    alloc.deallocate(p);
    CHECK(alloc.numBlocks() == 1); // block stays registered, just marked free
}

TEST_CASE("BlockAllocator_MultipleBlocks") {
    BlockAllocator alloc;

    void* a = alloc.allocate(1024);
    void* b = alloc.allocate(1024);
    void* c = alloc.allocate(1024);

    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(c != nullptr);
    CHECK(a != b);
    CHECK(b != c);
    CHECK(alloc.numBlocks() == 3);

    alloc.deallocate(b);
    alloc.deallocate(a);
    alloc.deallocate(c);
}

TEST_CASE("BlockAllocator_Alignment") {
    BlockAllocator alloc;

    void* p = alloc.allocate(4096, 64 * 1024); // 64 KiB alignment
    CHECK(p != nullptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    CHECK((addr % (64 * 1024)) == 0);

    alloc.deallocate(p);
}

TEST_CASE("BlockAllocator_DefaultBlockSize") {
    BlockAllocator alloc;

    // Requesting less than default block size still returns a full block
    void* p = alloc.allocate(1);
    CHECK(p != nullptr);
    CHECK(alloc.getAllocationSize(p) == BlockAllocator::DEFAULT_BLOCK_SIZE);

    alloc.deallocate(p);
}

TEST_CASE("BlockAllocator_TooLargeRequest") {
    BlockAllocator alloc;

    // Request larger than default block size -> nullptr
    void* p = alloc.allocate(BlockAllocator::DEFAULT_BLOCK_SIZE + 1);
    CHECK(p == nullptr);
}

// ---------------------------------------------------------------------------
// Thread safety
// ---------------------------------------------------------------------------
TEST_CASE("BlockAllocator_MultiThreaded") {
    BlockAllocator alloc;
    constexpr size_t THREADS = 8;
    constexpr size_t ALLOCS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::vector<void*> allPtrs[THREADS];

    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i) {
                void* p = alloc.allocate(4096);
                CHECK(p != nullptr);
                allPtrs[t].push_back(p);
            }
        });
    }

    for (auto& t : threads) t.join();

    CHECK(alloc.numBlocks() == THREADS * ALLOCS_PER_THREAD);

    // Deallocate everything
    for (size_t t = 0; t < THREADS; ++t) {
        for (void* p : allPtrs[t]) {
            alloc.deallocate(p);
        }
    }
}

// ---------------------------------------------------------------------------
// Memory write/read validation
// ---------------------------------------------------------------------------
TEST_CASE("BlockAllocator_MemoryIntegrity") {
    BlockAllocator alloc;

    void* p = alloc.allocate(4096);
    CHECK(p != nullptr);

    // Write pattern
    uint8_t* bytes = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < 4096; ++i) {
        bytes[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Verify
    for (size_t i = 0; i < 4096; ++i) {
        CHECK(bytes[i] == static_cast<uint8_t>(i & 0xFF));
    }

    alloc.deallocate(p);
}

// ---------------------------------------------------------------------------
// Stats consistency
// ---------------------------------------------------------------------------
TEST_CASE("BlockAllocator_StatsConsistency") {
    BlockAllocator alloc;

    size_t before = alloc.totalAllocated();
    CHECK(before == 0);

    void* p1 = alloc.allocate(1024);
    void* p2 = alloc.allocate(1024);

    size_t afterAlloc = alloc.totalAllocated();
    CHECK(afterAlloc == 2 * BlockAllocator::DEFAULT_BLOCK_SIZE);
    CHECK(alloc.totalUsed() == afterAlloc);

    alloc.deallocate(p1);
    CHECK(alloc.totalUsed() == BlockAllocator::DEFAULT_BLOCK_SIZE);
    CHECK(alloc.totalAllocated() == afterAlloc); // cumulative

    alloc.deallocate(p2);
    CHECK(alloc.totalUsed() == 0);
}
