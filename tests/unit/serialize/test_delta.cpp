#include <doctest/doctest.h>
#include "core/serialize/delta.h"

using namespace seed::serialize;

TEST_CASE("Delta_FloatArray_XOR") {
    float oldArr[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float newArr[5] = {1.0f, 2.5f, 3.0f, 4.0f, 5.1f};

    auto compressed = DeltaCompressor::compressFloatArray(oldArr, newArr, 5);
    CHECK(compressed.size() > 0);

    float outArr[5] = {};
    DeltaCompressor::decompressFloatArray(compressed.data(), compressed.size(), oldArr, outArr, 5);

    CHECK(outArr[0] == doctest::Approx(1.0f));
    CHECK(outArr[1] == doctest::Approx(2.5f));
    CHECK(outArr[2] == doctest::Approx(3.0f));
    CHECK(outArr[3] == doctest::Approx(4.0f));
    CHECK(outArr[4] == doctest::Approx(5.1f));
}

TEST_CASE("Delta_FloatArray_Unchanged") {
    float arr[3] = {1.0f, 2.0f, 3.0f};

    auto compressed = DeltaCompressor::compressFloatArray(arr, arr, 3);
    CHECK(compressed.size() == 8); // count + changedCount(0)

    float outArr[3] = {0.0f, 0.0f, 0.0f};
    DeltaCompressor::decompressFloatArray(compressed.data(), compressed.size(), arr, outArr, 3);

    CHECK(outArr[0] == doctest::Approx(1.0f));
    CHECK(outArr[1] == doctest::Approx(2.0f));
    CHECK(outArr[2] == doctest::Approx(3.0f));
}

TEST_CASE("Delta_FullFallback_SmallOld") {
    std::vector<uint8_t> oldData = {1, 2, 3};
    std::vector<uint8_t> newData = {10, 20, 30, 40, 50};

    auto delta = DeltaCompressor::compute(oldData, newData);
    auto result = DeltaCompressor::apply(oldData, delta);

    CHECK(result == newData);
}

TEST_CASE("Delta_ByteRange_Diff") {
    std::vector<uint8_t> oldData(100, 0);
    std::vector<uint8_t> newData(100, 0);
    newData[10] = 1;
    newData[11] = 2;
    newData[12] = 3;

    auto delta = DeltaCompressor::compute(oldData, newData);
    auto result = DeltaCompressor::apply(oldData, delta);

    CHECK(result == newData);
    CHECK(delta.size() < newData.size() / 2);
}

TEST_CASE("Delta_EmptyOld") {
    std::vector<uint8_t> oldData;
    std::vector<uint8_t> newData = {1, 2, 3, 4, 5};

    auto delta = DeltaCompressor::compute(oldData, newData);
    auto result = DeltaCompressor::apply(oldData, delta);

    CHECK(result == newData);
}


// ============================================================================
// Regression Tests for M06+TestQuality Fixes
// ============================================================================

#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/serialize/snapshot.h"

using namespace seed::memory;
using namespace seed::ecs;

struct DeltaPos { float x, y, z; };
SEED_REGISTER_COMPONENT_WITH_ID(DeltaPos, 200)

struct DeltaVel { float vx, vy, vz; };
SEED_REGISTER_COMPONENT_WITH_ID(DeltaVel, 201)

static void registerDeltaComponents() {
    seed::ecs::TypeRegistry::instance().registerComponent<DeltaPos>();
    seed::ecs::TypeRegistry::instance().registerComponent<DeltaVel>();
}

TEST_CASE("Delta_ComputeOrder_NewerMinusOlder") {
    // Regression for Bug 3: computeDelta() must be called on the NEWER snapshot.
    // Calling snap_old.computeDelta(snap_new) produces a logically inverted delta
    // that, when applied, reverts changes instead of forwarding them.
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerDeltaComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<DeltaPos>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<DeltaVel>(e, 0.1f, 0.2f, 0.3f);

    auto snapOld = Snapshot::capture(world);

    auto* pos = world.getComponent<DeltaPos>(e);
    pos->x = 99.0f;

    auto snapNew = Snapshot::capture(world);

    // Correct order: new.computeDelta(old)
    auto deltaCorrect = snapNew.computeDelta(snapOld);

    World world2(&blockAlloc);
    snapOld.apply(world2);
    deltaCorrect.apply(world2);

    auto* pos2 = world2.getComponent<DeltaPos>(e);
    REQUIRE(pos2 != nullptr);
    CHECK(pos2->x == doctest::Approx(99.0f));
    CHECK(pos2->y == doctest::Approx(2.0f));
    CHECK(pos2->z == doctest::Approx(3.0f));
}
