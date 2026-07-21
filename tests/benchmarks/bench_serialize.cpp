#include <doctest/doctest.h>
#include <chrono>
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/memory_system.h"

using namespace seed;

struct Position { float x, y, z; };
SEED_REGISTER_COMPONENT(Position);

struct Velocity { float x, y, z; };
SEED_REGISTER_COMPONENT(Velocity);

TEST_SUITE("bench_serialize") {

TEST_CASE("Snapshot_100k_Performance") {
    ecs::World world(&g_testAllocator);
    for (size_t i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto snap = serialize::Snapshot::capture(world);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    REQUIRE(snap.size() < 50 * 1024 * 1024);
    REQUIRE(elapsed.count() < 100'000'000);
}

TEST_CASE("Snapshot_Deserialize_100k_Performance") {
    ecs::World world(&g_testAllocator);
    for (size_t i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    auto snap = serialize::Snapshot::capture(world);

    ecs::World world2(&g_testAllocator);
    auto start = std::chrono::high_resolution_clock::now();
    snap.apply(world2);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    REQUIRE(elapsed.count() < 50'000'000);
}

TEST_CASE("Delta_1PercentChange_95PercentCompression") {
    ecs::World world(&g_testAllocator);
    for (size_t i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    auto snap1 = serialize::Snapshot::capture(world);

    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        if (rand() % 100 == 0) {
            pos->x += 1.0f;
            vel->x += 0.5f;
        }
    }

    auto snap2 = serialize::Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    size_t fullSize = snap2.size();
    size_t deltaSize = delta.size();

    REQUIRE(deltaSize < 100 * 1024);
    REQUIRE(deltaSize < fullSize / 20);
}

} // TEST_SUITE
