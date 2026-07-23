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

// GAP-FIX (2026-07-22, P0-3): SEED_ASSERT ist bewusst nur fuer interne
// Debug-Invarianten gedacht und wird im Release-Build (NDEBUG) zu einem
// No-Op (siehe core/profiling/seed_assert.h). Der CI-Schritt "Performance
// Gates" fuehrt dieses Benchmark aber genau im Release-Build aus - dadurch
// hat das Gate bisher NICHTS durchgesetzt (lokal nachgestellt: Deserialize
// 57ms trotz 50ms-Budget, Delta ~1MB trotz 100KB-Budget, Programm meldete
// trotzdem "All budgets passed" und exit 0). BENCH_CHECK ist unabhaengig
// von NDEBUG immer aktiv und setzt bei einer Verletzung den Exit-Code auf 1.
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

    // Modify ~1% of entities for delta test
    // GAP-FIX (2026-07-22, P1-1): vorher `% 100 < 16` - traf im Schnitt ~16%
    // der Entities statt der beabsichtigten ~1%, dadurch war die gemessene
    // Delta-Groesse/Kompressionsrate deutlich schlechter als in der Spec
    // vorgesehen. Deterministischer Zaehler statt Adress-Modulo.
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

    // GAP-FIX (2026-07-22, P0-3): Exit-Code spiegelt jetzt den tatsaechlichen
    // Budget-Status wider, unabhaengig von NDEBUG/Build-Typ.
    if (g_benchmarkFailed) {
        std::cerr << "One or more Monat 5 performance budgets FAILED.\n";
        return 1;
    }
    std::cout << "All Monat 5 performance budgets passed.\n";
    return 0;
}
