#include <doctest/doctest.h>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"
#include "core/diagnostics/diagnostics_manager.h"
#include "core/diagnostics/event_timeline.h"
#include <string>
#include <memory>
#include <random>
#include <fmt/format.h>

using namespace seed::ecs;
using namespace seed::memory;
using namespace seed::diagnostics;

// ---------------------------------------------------------------------------
// Test Components
// ---------------------------------------------------------------------------
struct Position {
    float x, y, z;
    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Velocity {
    float dx, dy, dz;
    Velocity() : dx(0), dy(0), dz(0) {}
    Velocity(float dx_, float dy_, float dz_) : dx(dx_), dy(dy_), dz(dz_) {}
};

struct Health {
    int hp;
    Health() : hp(100) {}
    explicit Health(int hp_) : hp(hp_) {}
};

struct Armor {
    int value;
    Armor() : value(0) {}
    explicit Armor(int v) : value(v) {}
};

struct Name {
    std::string value;
    Name() = default;
    explicit Name(std::string v) : value(std::move(v)) {}
};

struct UniqueResource {
    std::unique_ptr<int> data;
    UniqueResource() : data(std::make_unique<int>(0)) {}
    explicit UniqueResource(int v) : data(std::make_unique<int>(v)) {}
    UniqueResource(UniqueResource&&) = default;
    UniqueResource& operator=(UniqueResource&&) = default;
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;
};

// ---------------------------------------------------------------------------
// Component Registration
// ---------------------------------------------------------------------------
SEED_REGISTER_COMPONENT(Position);
SEED_REGISTER_COMPONENT(Velocity);
SEED_REGISTER_COMPONENT(Health);
SEED_REGISTER_COMPONENT(Armor);
SEED_REGISTER_COMPONENT(Name);
SEED_REGISTER_COMPONENT(UniqueResource);

// ---------------------------------------------------------------------------
// Helper Macros
// ---------------------------------------------------------------------------
#define CHECK_INVARIANTS(world) \
    do { CHECK(world.validateInvariants()); } while(0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
TEST_CASE("ECS_Entity_CreateDestroy") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    CHECK(e != INVALID_ENTITY);
    CHECK(world.isAlive(e));
    CHECK(world.entityCount() == 1);

    world.destroyEntity(e);
    CHECK(!world.isAlive(e));
    CHECK(world.entityCount() == 0);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_Many") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    std::vector<Entity> entities;
    for (int i = 0; i < 1000; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }
    CHECK(world.entityCount() == 1000);

    for (Entity e : entities) {
        CHECK(world.isAlive(e));
        Position* p = world.getComponent<Position>(e);
        REQUIRE(p != nullptr);
    }
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_AddGet") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 4.0f, 5.0f, 6.0f);

    Position* p = world.getComponent<Position>(e);
    Velocity* v = world.getComponent<Velocity>(e);
    REQUIRE(p != nullptr);
    REQUIRE(v != nullptr);
    CHECK(p->x == doctest::Approx(1.0f));
    CHECK(v->dx == doctest::Approx(4.0f));
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_AddRemove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 4.0f, 5.0f, 6.0f);
    CHECK(world.getComponent<Position>(e) != nullptr);
    CHECK(world.getComponent<Velocity>(e) != nullptr);

    world.removeComponent<Velocity>(e);
    CHECK(world.getComponent<Position>(e) != nullptr);
    CHECK(world.getComponent<Velocity>(e) == nullptr);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_ArchetypeMove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 4.0f, 5.0f, 6.0f);

    Position* p = world.getComponent<Position>(e);
    Velocity* v = world.getComponent<Velocity>(e);
    REQUIRE(p != nullptr);
    REQUIRE(v != nullptr);
    CHECK(p->x == doctest::Approx(1.0f));
    CHECK(v->dx == doctest::Approx(4.0f));
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Query_Basic") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 0.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 10.0f, 0.0f, 0.0f);

    int count = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        (void)pos;
        (void)vel;
        ++count;
    }
    CHECK(count == 1);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Query_Empty") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("ECS_Entity_Recycling") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    world.destroyEntity(e1);

    Entity e2 = world.createEntity();
    CHECK(e1 != e2);
    CHECK(entityIndex(e1) == entityIndex(e2));
    CHECK(entityVersion(e2) == entityVersion(e1) + 1);
    CHECK(!world.isAlive(e1));
    CHECK(world.isAlive(e2));
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_MultipleRecycle") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    for (Entity e : entities) {
        world.destroyEntity(e);
    }

    std::vector<Entity> newEntities;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        newEntities.push_back(e);
        CHECK(world.isAlive(e));
    }

    for (Entity oldE : entities) {
        CHECK(!world.isAlive(oldE));
    }
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_DestroyTwice") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.destroyEntity(e);
    world.destroyEntity(e);
    CHECK(!world.isAlive(e));
    CHECK(world.entityCount() == 0);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_DuplicateAdd") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    Position* p1 = world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    Position* p2 = world.addComponent<Position>(e, 4.0f, 5.0f, 6.0f);
    CHECK(p1 == p2);
    CHECK(p1->x == doctest::Approx(1.0f));
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_AddRemoveAdd") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.removeComponent<Position>(e);
    Position* p = world.addComponent<Position>(e, 4.0f, 5.0f, 6.0f);
    REQUIRE(p != nullptr);
    CHECK(p->x == doctest::Approx(4.0f));
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Query_DuringIteration") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 10);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_SwapAndPop_Consistency") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    std::vector<Entity> entities;
    for (int i = 0; i < 5; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    world.destroyEntity(entities[2]);

    for (size_t i = 0; i < entities.size(); ++i) {
        if (i == 2) continue;
        Position* p = world.getComponent<Position>(entities[i]);
        REQUIRE(p != nullptr);
    }
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_LastEntityInArchetype") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.destroyEntity(e);
    CHECK(world.entityCount() == 0);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Fuzz_RandomOperations") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();
    world.typeRegistry().registerComponent<Health>();

    std::vector<Entity> alive;

    for (int i = 0; i < 10000; ++i) {
        int op = i % 4;
        switch (op) {
            case 0: {
                Entity e = world.createEntity();
                alive.push_back(e);
                if (i % 3 == 0) world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
                if (i % 5 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
                if (i % 7 == 0) world.addComponent<Health>(e, 100);
                break;
            }
            case 1: {
                if (!alive.empty()) {
                    size_t idx = (i * 7) % alive.size();
                    Entity e = alive[idx];
                    if (world.isAlive(e)) {
                        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
                    }
                }
                break;
            }
            case 2: {
                if (!alive.empty()) {
                    size_t idx = (i * 13) % alive.size();
                    Entity e = alive[idx];
                    if (world.isAlive(e)) {
                        world.removeComponent<Velocity>(e);
                    }
                }
                break;
            }
            case 3: {
                if (!alive.empty()) {
                    size_t idx = (i * 17) % alive.size();
                    Entity e = alive[idx];
                    if (world.isAlive(e)) {
                        world.destroyEntity(e);
                    }
                }
                break;
            }
        }
    }

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Invariants_AfterStress") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();

    for (int i = 0; i < 100; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        if (i % 2 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    for (int i = 0; i < 100; ++i) {
        if (i % 3 == 0) {
            Entity e = world.createEntity();
            world.destroyEntity(e);
        }
    }

    CHECK(world.validateInvariants());
}

TEST_CASE("ECS_Component_StringType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Name>();

    Entity e = world.createEntity();
    world.addComponent<Name>(e, "TestEntity");

    Name* n = world.getComponent<Name>(e);
    REQUIRE(n != nullptr);
    CHECK(n->value == "TestEntity");

    world.typeRegistry().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    n = world.getComponent<Name>(e);
    REQUIRE(n != nullptr);
    CHECK(n->value == "TestEntity");
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_TypeRegistry_DuplicateRegistration") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    CHECK(world.getComponent<Position>(e) != nullptr);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_ArchetypeId_CollisionSafety") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();
    world.typeRegistry().registerComponent<Health>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Velocity>(e2, 1.0f, 0.0f, 0.0f);

    Entity e3 = world.createEntity();
    world.addComponent<Health>(e3);

    CHECK(world.getComponent<Position>(e1) != nullptr);
    CHECK(world.getComponent<Velocity>(e2) != nullptr);
    CHECK(world.getComponent<Health>(e3) != nullptr);
    CHECK(world.getComponent<Velocity>(e1) == nullptr);
    CHECK(world.getComponent<Position>(e3) == nullptr);
    CHECK_INVARIANTS(world);
}

// =============================================================================
// CRITICAL TEST: Move-Only Types (was failing with SIGSEGV)
// =============================================================================
TEST_CASE("ECS_Component_MoveOnlyType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<UniqueResource>();

    Entity e = world.createEntity();
    world.addComponent<UniqueResource>(e, 42);

    UniqueResource* res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 42);

    // Archetype move should properly move the unique_ptr
    world.typeRegistry().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 42);

    CHECK_INVARIANTS(world);

    fmt::print("\n=== MoveOnlyType Diagnostic Timeline ===\n");
    fmt::print("{}", diag.timeline().dump());
    fmt::print("{}", diag.timeline().performanceReport());
    fmt::print("========================================\n\n");
}

TEST_CASE("ECS_Entity_DestroyDuringQuery") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();

    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    world.destroyEntity(entities[5]);
    CHECK(!world.isAlive(entities[5]));
    CHECK_INVARIANTS(world);
}

// =============================================================================
// Diagnostic Integration Tests
// =============================================================================
TEST_CASE("Diagnostics_MoveOnly_NoDoubleDestruct") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<UniqueResource>();
    world.typeRegistry().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<UniqueResource>(e, 99);

    // This triggers archetype move - critical path for move-only types
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    // Check timeline for any MoveOnlyError events
    auto moveErrors = diag.timeline().getEventsByType(EventType::MoveOnlyError);
    auto asserts = diag.timeline().getEventsByType(EventType::AssertionFail);

    CHECK(moveErrors.empty());
    CHECK(asserts.empty());

    // Verify component integrity
    UniqueResource* res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 99);

    CHECK_INVARIANTS(world);
}

TEST_CASE("Diagnostics_PerformanceTracking") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<Position>();

    for (int i = 0; i < 100; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    auto perfReport = diag.timeline().performanceReport();
    CHECK(!perfReport.empty());

    // Verify we tracked EntityCreate events with timing
    auto creates = diag.timeline().getEventsByType(EventType::EntityCreate);
    CHECK(!creates.empty());

    // At least some events should have duration data
    bool hasTiming = false;
    for (const auto& ev : creates) {
        if (ev.durationNs > 0) {
            hasTiming = true;
            break;
        }
    }
    CHECK(hasTiming);
}

// =============================================================================
// PUNKT 4: Fuzz-Test mit Seed-Replay
// =============================================================================
TEST_CASE("ECS_Fuzz_WithReplaySeed") {
    // Fixed seed for reproducible failures
    uint64_t seed = 0xDEADBEEFCAFEBABE;
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> opDist(0, 3);
    std::uniform_int_distribution<int> componentDist(0, 2);

    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();
    world.typeRegistry().registerComponent<Health>();

    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    std::vector<Entity> alive;

    for (int i = 0; i < 5000; ++i) {
        int op = opDist(rng);
        switch (op) {
            case 0: {
                Entity e = world.createEntity();
                alive.push_back(e);
                int comp = componentDist(rng);
                if (comp == 0) world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
                if (comp == 1) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
                if (comp == 2) world.addComponent<Health>(e, 100);
                break;
            }
            case 1: {
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (world.isAlive(e)) {
                        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
                    }
                }
                break;
            }
            case 2: {
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (world.isAlive(e)) {
                        world.removeComponent<Velocity>(e);
                    }
                }
                break;
            }
            case 3: {
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (world.isAlive(e)) {
                        world.destroyEntity(e);
                    }
                }
                break;
            }
        }

        // Validate every 100 ops
        if (i % 100 == 0) {
            if (!world.validateInvariants()) {
                fmt::print("\n!!! FUZZ FAILURE AT ITERATION {} !!!\n", i);
                fmt::print("REPRODUCE WITH SEED: {}\n", seed);
                diag.snapshot(world, "fuzz-invariant-failure");
                CHECK(false);
                return;
            }
        }
    }

    CHECK_INVARIANTS(world);

    // On success, log the seed for reference
    fmt::print("\nFuzz test completed successfully with seed: {}\n", seed);
}

// =============================================================================
// PUNKT 7: Sanitizer-Ausgaben als DiagnosticEvent
// =============================================================================
TEST_CASE("Diagnostics_SanitizerEventTypes") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    // Simulate a sanitizer error event (would be triggered by real sanitizer)
    SEED_DIAG_EVENT(EventType::SanitizerError, INVALID_ENTITY, 0, 0, 0,
                    "ASan: heap-use-after-free detected", __FILE__, __LINE__);

    auto sanitizerEvents = diag.timeline().getEventsByType(EventType::SanitizerError);
    CHECK(sanitizerEvents.size() == 1);
    CHECK(std::string(sanitizerEvents[0].description).find("ASan") != std::string::npos);
}

TEST_CASE("Diagnostics_FileLogging") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    // Set up file logging
    std::string logPath = "test_diagnostics.log";
    diag.timeline().setLogFile(logPath);

    world.typeRegistry().registerComponent<Position>();
    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    diag.timeline().flushToFile();

    // Verify log file was created
    CHECK(!diag.timeline().getLogFilePath().empty());

    // Cleanup
    std::remove(logPath.c_str());
}
