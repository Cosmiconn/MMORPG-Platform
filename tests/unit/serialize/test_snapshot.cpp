#include <doctest/doctest.h>
#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/type_registry.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/serialize/binary_reader.h"

using namespace seed;
using namespace seed::memory;
using namespace seed::ecs;
using namespace seed::serialize;

// ---------------------------------------------------------------------------
// Snapshot-Test-Komponenten (eindeutige Namen, um ODR-Verletzungen mit
// den ECS-Tests zu vermeiden, die Position/Velocity/Health mit IDs 1-3
// definieren).
// ---------------------------------------------------------------------------
struct SnapPosition { float x, y, z; };
SEED_REGISTER_COMPONENT_WITH_ID(SnapPosition, 100)

struct SnapVelocity { float vx, vy, vz; };
SEED_REGISTER_COMPONENT_WITH_ID(SnapVelocity, 101)

struct SnapHealth { int32_t hp; int32_t maxHp; };
SEED_REGISTER_COMPONENT_WITH_ID(SnapHealth, 102)

static void registerSnapshotComponents() {
    TypeRegistry::instance().registerComponent<SnapPosition>();
    TypeRegistry::instance().registerComponent<SnapVelocity>();
    TypeRegistry::instance().registerComponent<SnapHealth>();
}

// ---------------------------------------------------------------------------

TEST_CASE("Snapshot_Capture_Size") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);

    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e1, 0.1f, 0.2f, 0.3f);

    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<SnapHealth>(e2, 100, 200);

    CHECK(world.entityCount() == 2);

    auto snap = Snapshot::capture(world);
    CHECK(snap.size() > sizeof(SnapshotHeader));
}

TEST_CASE("Snapshot_HeaderValid") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<SnapPosition>(e, 0.0f, 0.0f, 0.0f);

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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<SnapPosition>(e, 1.0f, 2.0f, 3.0f);

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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();

    SnapPosition p{5.0f, 6.0f, 7.0f};
    world.addComponentRaw(e, ComponentTraits<SnapPosition>::id, &p);

    auto* retrieved = world.getComponent<SnapPosition>(e);
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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();

    SnapPosition p{1.0f, 2.0f, 3.0f};
    SnapVelocity v{0.1f, 0.2f, 0.3f};

    world.addComponentRaw(e, ComponentTraits<SnapPosition>::id, &p);
    world.addComponentRaw(e, ComponentTraits<SnapVelocity>::id, &v);

    auto* pos = world.getComponent<SnapPosition>(e);
    auto* vel = world.getComponent<SnapVelocity>(e);
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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e1, 0.1f, 0.2f, 0.3f);

    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<SnapHealth>(e2, 100, 200);

    auto snap = Snapshot::capture(world);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap.apply(world2);

    CHECK(world2.entityCount() == 2);

    int posVelCount = 0;
    for (auto [pos, vel] : world2.query<SnapPosition, SnapVelocity>()) {
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
    for (auto [pos, health] : world2.query<SnapPosition, SnapHealth>()) {
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
    registerSnapshotComponents();

    World world(&blockAlloc);
    constexpr int N = 1000;
    for (int i = 0; i < N; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3));
        if (i % 2 == 0) world.addComponent<SnapVelocity>(e, 1.0f, 0.0f, 0.0f);
    }

    auto snap = Snapshot::capture(world);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap.apply(world2);

    CHECK(world2.entityCount() == N);

    int posCount = 0;
    int posVelCount = 0;
    for (auto [pos] : world2.query<SnapPosition>()) {
        (void)pos;
        posCount++;
    }
    for (auto [pos, vel] : world2.query<SnapPosition, SnapVelocity>()) {
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
    registerSnapshotComponents();

    World world(&blockAlloc);

    // Baseline entities: delta overhead is amortized across many entities
    for (int i = 0; i < 10; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e1, 0.1f, 0.2f, 0.3f);

    auto snap1 = Snapshot::capture(world);

    // Modify one component of one entity
    auto* pos = world.getComponent<SnapPosition>(e1);
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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);

    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<SnapHealth>(e2, 50, 100);

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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);

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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e1, 0.1f, 0.2f, 0.3f);

    auto snap1 = Snapshot::capture(world);

    // Modify
    auto* pos = world.getComponent<SnapPosition>(e1);
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
    for (auto [pos_q] : world2.query<SnapPosition>()) {
        preCount++;
        CHECK(pos_q->x == doctest::Approx(1.0f));
    }
    CHECK(preCount == 1);

    // Apply delta
    delta.apply(world2);

    // Verify post-delta state
    int postCount = 0;
    for (auto [pos_q] : world2.query<SnapPosition>()) {
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
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);

    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<SnapHealth>(e2, 50, 100);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    BlockAllocator blockAlloc2;
    g_blockAllocator = &blockAlloc2;
    World world2(&blockAlloc2);
    snap1.apply(world2);

    CHECK(world2.entityCount() == 1);

    delta.apply(world2);

    CHECK(world2.entityCount() == 2);

    int found = 0;
    for (auto [pos, health] : world2.query<SnapPosition, SnapHealth>()) {
        found++;
        CHECK(pos->x == doctest::Approx(10.0f));
        CHECK(health->hp == 50);
        CHECK(health->maxHp == 100);
    }
    CHECK(found == 1);
}

TEST_CASE("Delta_Apply_RemovedEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);

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

    int found = 0;
    for (auto [pos] : world2.query<SnapPosition>()) {
        found++;
        CHECK(pos->x == doctest::Approx(1.0f));
    }
    CHECK(found == 1);
}

// ---------------------------------------------------------------------------
// Performance & Budget Tests (Monat 5 Acceptance Criteria)
// ---------------------------------------------------------------------------

TEST_CASE("Snapshot_Performance_100k_Entities") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    for (int i = 0; i < 100000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto snap = Snapshot::capture(world);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;

    CHECK(ms < 100.0); // < 100 ms
    CHECK(snap.serialize().size() < 50 * 1024 * 1024); // < 50 MB
}

TEST_CASE("Snapshot_Deserialize_Performance_100k") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    for (int i = 0; i < 100000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto snap = Snapshot::capture(world);
    auto data = snap.serialize();

    World world2(&blockAlloc);
    auto start = std::chrono::high_resolution_clock::now();
    auto snap2 = Snapshot::deserialize(data);
    snap2.apply(world2);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;

    CHECK(ms < 50.0); // < 50 ms
    CHECK(world2.entityCount() == 100000);
}

TEST_CASE("Delta_Compression_1PercentChange") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    for (int i = 0; i < 10000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto snap1 = Snapshot::capture(world);

    // Modify 1% of entities (100 entities)
    int modified = 0;
    for (auto [pos, vel] : world.query<SnapPosition, SnapVelocity>()) {
        if (modified++ >= 100) break;
        pos->x += 1.0f;
        vel->vx += 0.01f;
    }

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    auto snapSize = snap2.serialize().size();
    auto deltaSize = delta.serialize().size();

    // Delta should be significantly smaller than full snapshot
    // With float-array XOR compression, 100 changed entities * 2 components
    // should be ~100 * (overhead + compressed float array) << 50% of snapshot
    CHECK(deltaSize < snapSize / 2);
}
