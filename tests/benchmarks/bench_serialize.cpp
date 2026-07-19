#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include <chrono>
#include <iostream>

using namespace seed;
using namespace seed::memory;
using namespace seed::ecs;
using namespace seed::serialize;

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

    World world(&blockAlloc);

    constexpr size_t N = 100'000;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < N; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), static_cast<float>(i*2), static_cast<float>(i*3));
        if (i % 2 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        if (i % 3 == 0) world.addComponent<Health>(e, 100, 100);
    }

    auto createEnd = std::chrono::high_resolution_clock::now();
    std::cout << "Created " << N << " entities in "
              << std::chrono::duration<double, std::milli>(createEnd - start).count() << " ms\n";

    auto snapStart = std::chrono::high_resolution_clock::now();
    auto snap = Snapshot::capture(world);
    auto snapEnd = std::chrono::high_resolution_clock::now();

    std::cout << "Snapshot: " << snap.size() << " bytes in "
              << std::chrono::duration<double, std::milli>(snapEnd - snapStart).count() << " ms\n";

    for (auto [pos] : world.query<Position>()) {
        if (reinterpret_cast<uintptr_t>(pos) % 100 < 16) {
            pos->x += 1.0f;
        }
    }

    auto snap2 = Snapshot::capture(world);
    auto deltaStart = std::chrono::high_resolution_clock::now();
    auto delta = DeltaCompressor::compute(snap.data(), snap2.data());
    auto deltaEnd = std::chrono::high_resolution_clock::now();

    std::cout << "Delta: " << delta.size() << " bytes in "
              << std::chrono::duration<double, std::milli>(deltaEnd - deltaStart).count() << " ms\n";
    std::cout << "Compression ratio: " << (100.0 * static_cast<double>(delta.size()) / static_cast<double>(snap2.size())) << "%\n";

    return 0;
}

TEST_CASE("Benchmark_Snapshot_100k_Budget") {
    // Validates Monat 5 acceptance criteria on target hardware.
    // Run locally: ./seed_bench_serialize --test-case="Benchmark_Snapshot_100k_Budget"
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    seed::ecs::TypeRegistry::instance().registerComponent<SnapPosition>();
    seed::ecs::TypeRegistry::instance().registerComponent<SnapVelocity>();

    World world(&blockAlloc);
    for (int i = 0; i < 100000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto snap = Snapshot::capture(world);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) / 1000.0;

    CHECK(ms < 100.0); // Monat 5 spec: < 100 ms
    CHECK(snap.serialize().size() < 50 * 1024 * 1024); // < 50 MB

    auto data = snap.serialize();
    World world2(&blockAlloc);
    start = std::chrono::high_resolution_clock::now();
    auto snap2 = Snapshot::deserialize(data);
    snap2.apply(world2);
    elapsed = std::chrono::high_resolution_clock::now() - start;
    ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) / 1000.0;

    CHECK(ms < 50.0); // Monat 5 spec: < 50 ms
    CHECK(world2.entityCount() == 100000);
}
