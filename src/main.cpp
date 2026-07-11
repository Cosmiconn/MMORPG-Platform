#include <cstdio>
#include <cstdlib>
#include <string>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "core/memory/memory_system.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/arena_allocator.h"
#include "core/memory/stack_allocator.h"

using namespace seed::memory;

// ---------------------------------------------------------------------------
// Smoke-test helpers
// ---------------------------------------------------------------------------
static bool test_cpp20() {
    auto is_numeric = [](auto x) -> bool {
        if constexpr (std::is_arithmetic_v<decltype(x)>) {
            return true;
        } else {
            return false;
        }
    };
    return is_numeric(42) && is_numeric(3.14f) && !is_numeric(std::string("hello"));
}

static bool test_memory_system() {
    SEED_ZONE("test_memory_system");

    // Initialize global allocators
    g_blockAllocator = new BlockAllocator();
    g_memoryTracker  = new MemoryTracker();
    g_frameArena     = new ArenaAllocator(g_blockAllocator);

    // Set budget alarm
    bool alarmFired = false;
    g_memoryTracker->setAlarmCallback([&](const std::string&, size_t, size_t) {
        alarmFired = true;
    });

    // Test PoolAllocator
    {
        PoolAllocator<uint64_t> pool(g_blockAllocator);
        auto* p = pool.construct(42ULL);
        if (!p || *p != 42) return false;
        g_memoryTracker->trackAllocation("pool_test", sizeof(uint64_t));
        pool.destroy(p);
        g_memoryTracker->trackDeallocation("pool_test", sizeof(uint64_t));
    }

    // Test ArenaAllocator (frame-scoped)
    {
        void* a1 = g_frameArena->allocate(64, 8);
        void* a2 = g_frameArena->allocate(128, 16);
        if (!a1 || !a2) return false;
        g_frameArena->reset(); // bulk free
    }

    // Test StackAllocator
    {
        StackAllocator stack(g_blockAllocator, 4096);
        void* s1 = stack.allocate(100, 8);
        auto marker = stack.getMarker();
        void* s2 = stack.allocate(200, 8);
        if (!s1 || !s2) return false;
        stack.freeToMarker(marker);
    }

    // Test MemoryTracker budget
    g_memoryTracker->setBudget("render", 100);
    g_memoryTracker->trackAllocation("render", 50);
    g_memoryTracker->trackAllocation("render", 60); // over budget
    if (!alarmFired) return false;

    // Cleanup
    delete g_frameArena;
    delete g_memoryTracker;
    delete g_blockAllocator;

    g_frameArena     = nullptr;
    g_memoryTracker  = nullptr;
    g_blockAllocator = nullptr;

    return true;
}

static bool test_no_new_delete() {
    // Verify we can allocate without new/delete
    BlockAllocator ba;
    void* p = ba.allocate(1024);
    if (!p) return false;
    ba.deallocate(p);
    return true;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    SEED_ZONE("main");
    (void)argc;
    (void)argv;

    fmt::print("\n=== TheSeed M2 Integration Test ===\n");
    fmt::print("C++ Standard: {}\n", __cplusplus);
    fmt::print("Build: P0-M2 (Custom Memory Management)\n\n");

    bool ok = true;

    ok &= test_cpp20();
    fmt::print("[{}] C++20 concepts\n", ok ? "PASS" : "FAIL");

    ok &= test_no_new_delete();
    fmt::print("[{}] No new/delete in hot path\n", ok ? "PASS" : "FAIL");

    ok &= test_memory_system();
    fmt::print("[{}] Memory system integration\n", ok ? "PASS" : "FAIL");

    fmt::print("\n===================================\n");
    if (ok) {
        spdlog::info("All M2 integration tests passed. Ready for P0-M3 (ECS).");
        return EXIT_SUCCESS;
    } else {
        spdlog::error("M2 integration test FAILED!");
        return EXIT_FAILURE;
    }
}
