#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/profiling/seed_assert.h"
#include <chrono>
#include <iostream>

using namespace seed;
using namespace seed::memory;
using namespace seed::ecs;
using namespace seed::serialize;

// GAP-FIX (Performance-Gate war im Release-Build wirkungslos): eigenes,
// von NDEBUG unabhaengiges Check-Makro fuer Benchmark-Budgets. SEED_ASSERT
// ist bewusst nur fuer Debug-Invarianten gedacht und wird im Release-Build
// (NDEBUG) zu einem No-Op - für CI-Performance-Gates ungeeignet.
static bool g_benchmarkFailed = false;
#define BENCH_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "BENCHMARK BUDGET FAIL: " << (msg) << "\n"; \
            g_benchmarkFailed = true; \
        } \
    } while (0)

struct Position { float x, y, z; };
SEED_REGISTER_COMPONENT_WITH_ID(Position, 100)

struct Velocity { float vx, vy, vz; };
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 101)

struct Health { int32_t hp; int32_t maxHp; };
SEED_REGISTER_COMPONENT_WITH_ID(Health, 102)

int main() {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    // Register components in ECS TypeRegistry so parseEntities()
    // can resolve component sizes during delta computation.
    seed::ecs::TypeRegistry::instance().registerComponent<Position>();
    seed::ecs::TypeRegistry::instance().registerComponent<Velocity>();
    seed::ecs::TypeRegistry::instance().registerComponent<Health>();

    World world(&blockAlloc);

    constexpr size_t N = 100'000;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < N; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        if (i % 3 == 0) world.addComponent<Health>(e, 100, 100);
    }

    auto createEnd = std::chrono::high_resolution_clock::now();
    std::cout << "Created " << N << " entities in "
              << std::chrono::duration<double, std::milli>(createEnd - start).count() << " ms\n";

    // --- Snapshot capture performance (Monat 5 spec: < 100 ms) ---
    auto snapStart = std::chrono::high_resolution_clock::now();
    auto snap = Snapshot::capture(world);
    auto snapEnd = std::chrono::high_resolution_clock::now();
    double snapMs = std::chrono::duration<double, std::milli>(snapEnd - snapStart).count();

    std::cout << "Snapshot: " << snap.serialize().size() << " bytes in " << snapMs << " ms\n";
    BENCH_CHECK(snapMs < 100.0, "Snapshot capture exceeded 100ms budget");
    BENCH_CHECK(snap.serialize().size() < 50ULL * 1024 * 1024, "Snapshot size exceeded 50MB budget");

    // Modify ~1% of entities for delta test (deterministischer Zaehler statt
    // Adress-Modulo - GAP-FIX: `% 100 < 16` traf vorher ~16%, nicht ~1%)
    size_t modifyTarget = N / 100;
    size_t modifiedCount = 0;
    for (auto [pos] : world.query<Position>()) {
        if (modifiedCount >= modifyTarget) break;
        pos->x += 1.0f;
        ++modifiedCount;
    }

    auto snap2 = Snapshot::capture(world);

    // --- Delta compression performance ---
    auto deltaStart = std::chrono::high_resolution_clock::now();
    // FIX (Bug 3): computeDelta must be called on the NEWER snapshot.
    auto delta = snap2.computeDelta(snap);
    auto deltaEnd = std::chrono::high_resolution_clock::now();
    double deltaMs = std::chrono::duration<double, std::milli>(deltaEnd - deltaStart).count();

    std::cout << "Delta: " << delta.size() << " bytes in " << deltaMs << " ms\n";
    std::cout << "Compression ratio: "
              << (100.0 * static_cast<double>(delta.size()) / static_cast<double>(snap2.serialize().size()))
              << "%\n";
    BENCH_CHECK(delta.size() < 100 * 1024, "Delta compression exceeded 100KB budget for 1% change");

    // --- Deserialization performance (Monat 5 spec: < 50 ms) ---
    auto data = snap.serialize();
    World world2(&blockAlloc);
    auto deserStart = std::chrono::high_resolution_clock::now();
    auto snap3 = Snapshot::deserialize(data);
    snap3.apply(world2);
    auto deserEnd = std::chrono::high_resolution_clock::now();
    double deserMs = std::chrono::duration<double, std::milli>(deserEnd - deserStart).count();

    std::cout << "Deserialize+Apply: " << deserMs << " ms\n";
    BENCH_CHECK(deserMs < 50.0, "Snapshot deserialize exceeded 50ms budget");
    BENCH_CHECK(world2.entityCount() == N, "Entity count mismatch after deserialization");

    if (g_benchmarkFailed) {
        std::cerr << "One or more Monat 5 performance budgets FAILED.\n";
        return 1;
    }
    std::cout << "All Monat 5 performance budgets passed.\n";
    return 0;
}
