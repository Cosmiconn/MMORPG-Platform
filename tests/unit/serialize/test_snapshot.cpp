#include <doctest/doctest.h>
#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"

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

TEST_CASE("Snapshot_Capture_Size") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);

    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e1, 0.1f, 0.2f, 0.3f);

    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<Health>(e2, 100, 200);

    CHECK(world.entityCount() == 2);

    auto snap = Snapshot::capture(world);
    CHECK(snap.size() > sizeof(SnapshotHeader));
}

TEST_CASE("Snapshot_HeaderValid") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<Position>(e, 0.0f, 0.0f, 0.0f);

    auto snap = Snapshot::capture(world);
    BinaryReader reader(snap.data());
    auto header = reader.readPOD<SnapshotHeader>();

    CHECK(header.magic == SnapshotHeader::MAGIC);
    CHECK(header.version == SnapshotHeader::VERSION);
    CHECK(header.entityCount == 1);
    CHECK(header.archetypeCount >= 1);
}

TEST_CASE("Snapshot_SerializeRoundtrip") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);
    auto bytes = snap1.serialize();
    auto snap2 = Snapshot::deserialize(bytes);

    CHECK(snap2.size() == snap1.size());
}

TEST_CASE("World_addComponentRaw") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e = world.createEntity();

    Position p{5.0f, 6.0f, 7.0f};
    world.addComponentRaw(e, ComponentTraits<Position>::id, &p);

    auto* retrieved = world.getComponent<Position>(e);
    REQUIRE(retrieved != nullptr);
    CHECK(retrieved->x == doctest::Approx(5.0f));
    CHECK(retrieved->y == doctest::Approx(6.0f));
    CHECK(retrieved->z == doctest::Approx(7.0f));
}

TEST_CASE("World_addComponentRaw_ArchetypeMove") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e = world.createEntity();

    Position p{1.0f, 2.0f, 3.0f};
    Velocity v{0.1f, 0.2f, 0.3f};

    world.addComponentRaw(e, ComponentTraits<Position>::id, &p);
    world.addComponentRaw(e, ComponentTraits<Velocity>::id, &v);

    auto* pos = world.getComponent<Position>(e);
    auto* vel = world.getComponent<Velocity>(e);
    REQUIRE(pos != nullptr);
    REQUIRE(vel != nullptr);
    CHECK(pos->x == doctest::Approx(1.0f));
    CHECK(vel->vx == doctest::Approx(0.1f));
}

TEST_CASE("Snapshot_ApplyRoundtrip") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e1, 0.1f, 0.2f, 0.3f);

    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<Health>(e2, 100, 200);

    auto snap = Snapshot::capture(world);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap.apply(world2);

    CHECK(world2.entityCount() == 2);

    int posVelCount = 0;
    for (auto [pos, vel] : world2.query<Position, Velocity>()) {
        posVelCount++;
        CHECK(pos->x == doctest::Approx(1.0f));
        CHECK(pos->y == doctest::Approx(2.0f));
        CHECK(pos->z == doctest::Approx(3.0f));
        CHECK(vel->vx == doctest::Approx(0.1f));
        CHECK(vel->vy == doctest::Approx(0.2f));
        CHECK(vel->vz == doctest::Approx(0.3f));
    }
    CHECK(posVelCount == 1);

    int posHealthCount = 0;
    for (auto [pos, health] : world2.query<Position, Health>()) {
        posHealthCount++;
        CHECK(pos->x == doctest::Approx(10.0f));
        CHECK(pos->y == doctest::Approx(20.0f));
        CHECK(pos->z == doctest::Approx(30.0f));
        CHECK(health->hp == 100);
        CHECK(health->maxHp == 200);
    }
    CHECK(posHealthCount == 1);
}

TEST_CASE("Snapshot_Apply_1000_Entities") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    constexpr int N = 1000;
    for (int i = 0; i < N; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3));
        if (i % 2 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    auto snap = Snapshot::capture(world);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap.apply(world2);

    CHECK(world2.entityCount() == N);

    int posCount = 0;
    int posVelCount = 0;
    for (auto [pos] : world2.query<Position>()) {
        (void)pos;
        posCount++;
    }
    for (auto [pos, vel] : world2.query<Position, Velocity>()) {
        (void)pos;
        (void)vel;
        posVelCount++;
    }
    CHECK(posCount == N);
    CHECK(posVelCount == (N + 1) / 2);
}

TEST_CASE("Snapshot_ComputeDelta_ChangedComponents") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e1, 0.1f, 0.2f, 0.3f);

    auto snap1 = Snapshot::capture(world);

    // Modify one component
    auto* pos = world.getComponent<Position>(e1);
    pos->x = 99.0f;

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    CHECK(delta.size() > 0);
    CHECK(delta.size() < snap2.size() / 2); // Delta should be smaller

    auto header = delta.readHeader();
    CHECK(header.numChangedEntities == 1);
}

TEST_CASE("Snapshot_ComputeDelta_NewEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);

    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<Health>(e2, 50, 100);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    auto header = delta.readHeader();
    CHECK(header.numNewEntities == 1);
}

TEST_CASE("Snapshot_ComputeDelta_RemovedEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 20.0f, 30.0f);

    auto snap1 = Snapshot::capture(world);

    world.destroyEntity(e2);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    auto header = delta.readHeader();
    CHECK(header.numRemovedEntities == 1);
}

TEST_CASE("Delta_Apply_ChangedComponents") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e1, 0.1f, 0.2f, 0.3f);

    auto snap1 = Snapshot::capture(world);

    // Modify
    auto* pos = world.getComponent<Position>(e1);
    pos->x = 99.0f;

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    // Reset world to snap1 state via fresh world
    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap1.apply(world2);

    CHECK(world2.entityCount() == 1);

    // Verify pre-delta state
    int preCount = 0;
    for (auto [pos_q] : world2.query<Position>()) {
        preCount++;
        CHECK(pos_q->x == doctest::Approx(1.0f));
    }
    CHECK(preCount == 1);

    // Apply delta
    delta.apply(world2);

    // Verify post-delta state
    int postCount = 0;
    for (auto [pos_q] : world2.query<Position>()) {
        postCount++;
        CHECK(pos_q->x == doctest::Approx(99.0f));
    }
    CHECK(postCount == 1);
}

TEST_CASE("Delta_Apply_NewEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);

    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<Health>(e2, 50, 100);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap1.apply(world2);

    CHECK(world2.entityCount() == 1);

    delta.apply(world2);

    CHECK(world2.entityCount() == 2);

    int healthCount = 0;
    for (auto [h] : world2.query<Health>()) {
        healthCount++;
        CHECK(h->hp == 50);
        CHECK(h->maxHp == 100);
    }
    CHECK(healthCount == 1);
}

TEST_CASE("Delta_Apply_RemovedEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 20.0f, 30.0f);

    auto snap1 = Snapshot::capture(world);

    world.destroyEntity(e2);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap1.apply(world2);

    CHECK(world2.entityCount() == 2);

    delta.apply(world2);

    CHECK(world2.entityCount() == 1);
}
