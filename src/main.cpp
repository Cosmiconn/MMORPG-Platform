#include "core/memory/memory_system.h"
#include "core/profiling/tracy_seed.h"
#include <iostream>

using namespace seed::memory;

int main() {
    SEED_ZONE("main");

    // Initialize global allocators
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    ArenaAllocator frameArena(&blockAlloc);

    g_blockAllocator = &blockAlloc;
    g_memoryTracker  = &tracker;
    g_frameArena     = &frameArena;

    std::cout << "TheSeed – Phase 0 Fundament\n";
    std::cout << "BlockAllocator ready: " << blockAlloc.totalAllocated() << " bytes\n";

    // Smoke test: allocate and free via pool
    {
        PoolAllocator<uint64_t> pool(&blockAlloc);
        auto* p = pool.construct(42ULL);
        std::cout << "Pool value: " << *p << "\n";
        pool.destroy(p);
    }

    SEED_FRAME_MARK();
    return 0;
}
