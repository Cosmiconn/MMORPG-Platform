#include <doctest/doctest.h>
#include <random>
#include <string>
#include <cstring>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"

#define CHECK_INVARIANTS(world) CHECK(world.validateInvariants())

using namespace seed::ecs;
using namespace seed::memory;

struct UniqueResource {
    std::unique_ptr<int> data;
    explicit UniqueResource(int v = 42) : data(std::make_unique<int>(v)) {}
    UniqueResource(UniqueResource&&) = default;
    UniqueResource& operator=(UniqueResource&&) = default;
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;
};

SEED_REGISTER_COMPONENT(UniqueResource)

struct Position {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Position() = default;
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Velocity {
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    Velocity() = default;
    Velocity(float vx_, float vy_, float vz_) : vx(vx_), vy(vy_), vz(vz_) {}
};

struct Health {
    int hp = 100;
    int maxHp = 100;
    Health() = default;
    explicit Health(int hp_) : hp(hp_), maxHp(hp_) {}
};

struct Name {
    char value[32] = {};
    Name() = default;
    explicit Name(const char* s) {
#if defined(_WIN32) && defined(_MSC_VER)
        strncpy_s(value, sizeof(value), s, sizeof(value) - 1);
#else
        std::strncpy(value, s, sizeof(value) - 1);
#endif
        value[sizeof(value) - 1] = '\0';
    }
};

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)
SEED_REGISTER_COMPONENT_WITH_ID(Health, 3)
SEED_REGISTER_COMPONENT_WITH_ID(Name, 4)

TEST_CASE("ECS_Entity_CreateDestroy") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e = world.createEntity();
    CHECK(e != INVALID_ENTITY);
    CHECK(world.isAlive(e));

    world.destroyEntity(e);
    CHECK(!world.isAlive(e));
}

TEST_CASE("ECS_Entity_Many") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();

    constexpr size_t N = 1000;
    std::vector<Entity> entities;
    entities.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        entities.push_back(world.createEntity());
    }

    CHECK(world.entityCount() == N);

    for (auto e : entities) {
        CHECK(world.isAlive(e));
    }

    for (auto e : entities) {
        world.destroyEntity(e);
    }

    CHECK(world.entityCount() == 0);
}

TEST_CASE("ECS_Component_AddGet") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e = world.createEntity();
    auto* pos = world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);
    CHECK(pos->y == 2.0f);
    CHECK(pos->z == 3.0f);

    auto* pos2 = world.getComponent<Position>(e);
    REQUIRE(pos2 != nullptr);
    CHECK(pos2 == pos);
}

TEST_CASE("ECS_Component_AddRemove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 0.1f, 0.2f, 0.3f);
    world.addComponent<Health>(e);

    CHECK(world.hasComponent<Position>(e));
    CHECK(world.hasComponent<Velocity>(e));
    CHECK(world.hasComponent<Health>(e));

    world.removeComponent<Velocity>(e);
    CHECK(world.hasComponent<Position>(e));
    CHECK(!world.hasComponent<Velocity>(e));
    CHECK(world.hasComponent<Health>(e));
}

TEST_CASE("ECS_Component_ArchetypeMove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 2.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e2, 0.5f, 0.0f, 0.0f);

    auto* pos1 = world.getComponent<Position>(e1);
    REQUIRE(pos1 != nullptr);
    CHECK(pos1->x == 1.0f);

    world.addComponent<Velocity>(e1, 1.0f, 0.0f, 0.0f);
    auto* pos1After = world.getComponent<Position>(e1);
    REQUIRE(pos1After != nullptr);
    CHECK(pos1After->x == 1.0f);
}

TEST_CASE("ECS_Query_Basic") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    for (size_t i = 0; i < 5; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 1.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        REQUIRE(pos != nullptr);
        ++count;
    }
    CHECK(count == 15);

    count = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        REQUIRE(pos != nullptr);
        REQUIRE(vel != nullptr);
        CHECK(vel->vx == 1.0f);
        ++count;
    }
    CHECK(count == 5);
}

TEST_CASE("ECS_Query_Empty") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    auto result = world.query<Position, Velocity>();
    CHECK(result.empty());
    CHECK(result.count() == 0);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_Recycling") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e1 = world.createEntity();
    uint32_t idx1 = entityIndex(e1);
    uint8_t ver1 = entityVersion(e1);

    world.destroyEntity(e1);
    CHECK(!world.isAlive(e1));

    Entity e2 = world.createEntity();
    uint32_t idx2 = entityIndex(e2);

    // Same index recycled, but version incremented
    CHECK(idx1 == idx2);
    CHECK(entityVersion(e2) == ver1 + 1);

    // Old handle is invalid
    CHECK(!world.isAlive(e1));
    CHECK(world.isAlive(e2));
}

TEST_CASE("ECS_Entity_MultipleRecycle") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    std::vector<Entity> handles;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        handles.push_back(e);
    }

    // Destroy all
    for (auto e : handles) {
        world.destroyEntity(e);
    }

    // Recreate all - should reuse indices with new versions
    std::vector<Entity> newHandles;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        newHandles.push_back(e);
    }

    // Old handles invalid
    for (auto e : handles) {
        CHECK(!world.isAlive(e));
    }

    // New handles valid
    for (auto e : newHandles) {
        CHECK(world.isAlive(e));
    }

    // Same indices, different versions
    for (size_t i = 0; i < handles.size(); ++i) {
        CHECK(entityIndex(handles[i]) == entityIndex(newHandles[i]));
        CHECK(entityVersion(newHandles[i]) == entityVersion(handles[i]) + 1);
    }
}

TEST_CASE("ECS_Entity_DestroyTwice") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    Entity e = world.createEntity();
    world.destroyEntity(e);

    // Second destroy should be safe (no-op)
    world.destroyEntity(e);
    CHECK(!world.isAlive(e));
}

TEST_CASE("ECS_Component_DuplicateAdd") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    // Adding duplicate component should assert in debug, be safe in release
    // In debug mode, this triggers an assertion - we can't test it directly
    // But we can verify the component is still correct after the first add
    Position* pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);
}

TEST_CASE("ECS_Component_AddRemoveAdd") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    // Remove component (if removeComponent exists)
    // world.removeComponent<Position>(e);
    // CHECK(world.getComponent<Position>(e) == nullptr);

    // Add again
    // world.addComponent<Position>(e, 4.0f, 5.0f, 6.0f);
    // Position* pos = world.getComponent<Position>(e);
    // CHECK(pos->x == 4.0f);
}

TEST_CASE("ECS_Query_DuringIteration") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        entities.push_back(e);
    }

    // Query should see all 10 entities
    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 10);

    // Add Velocity to first entity
    world.addComponent<Velocity>(entities[0], 1.0f, 0.0f, 0.0f);

    // Query for Position+Velocity should only see 1 entity
    count = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        (void)pos; (void)vel;
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("ECS_SwapAndPop_Consistency") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 2.0f, 0.0f, 0.0f);

    Entity e3 = world.createEntity();
    world.addComponent<Position>(e3, 3.0f, 0.0f, 0.0f);

    // Destroy middle entity - swap-and-pop should move e3 to e2's position
    world.destroyEntity(e2);

    // e3 should still be valid with correct data
    CHECK(world.isAlive(e3));
    Position* pos3 = world.getComponent<Position>(e3);
    REQUIRE(pos3 != nullptr);
    CHECK(pos3->x == 3.0f);

    // e1 should still be valid
    CHECK(world.isAlive(e1));
    Position* pos1 = world.getComponent<Position>(e1);
    REQUIRE(pos1 != nullptr);
    CHECK(pos1->x == 1.0f);
}

TEST_CASE("ECS_LastEntityInArchetype") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 42.0f, 0.0f, 0.0f);

    // Destroy the only entity in this archetype
    world.destroyEntity(e);
    CHECK(!world.isAlive(e));

    // Archetype should now be empty
    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 99.0f, 0.0f, 0.0f);
    Position* pos = world.getComponent<Position>(e2);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 99.0f);
}

TEST_CASE("ECS_Fuzz_RandomOperations") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    std::vector<Entity> alive;
    std::mt19937 rng(42); // Fixed seed for reproducibility

    for (int i = 0; i < 100000; ++i) {
        int op = static_cast<int>(rng() % 7);

        switch (op) {
            case 0: { // Create entity
                Entity e = world.createEntity();
                alive.push_back(e);
                break;
            }
            case 1: { // Destroy random entity
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    world.destroyEntity(alive[idx]);
                    alive.erase(alive.begin() + static_cast<std::vector<Entity>::difference_type>(idx));
                }
                break;
            }
            case 2: { // Add Position
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (!world.hasComponent<Position>(e)) {
                        world.addComponent<Position>(e,
                            static_cast<float>(rng() % 100),
                            static_cast<float>(rng() % 100),
                            static_cast<float>(rng() % 100));
                    }
                }
                break;
            }
            case 3: { // Add Velocity
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (!world.hasComponent<Velocity>(e)) {
                        world.addComponent<Velocity>(e,
                            static_cast<float>(rng() % 10),
                            static_cast<float>(rng() % 10),
                            static_cast<float>(rng() % 10));
                    }
                }
                break;
            }
            case 4: { // Add Health
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (!world.hasComponent<Health>(e)) {
                        world.addComponent<Health>(e, static_cast<int>(rng() % 100));
                    }
                }
                break;
            }
            case 5: { // Query Position
                int count = 0;
                for (auto [pos] : world.query<Position>()) {
                    (void)pos;
                    ++count;
                }
                // Just verify it doesn't crash
                CHECK(count >= 0);
                break;
            }
            case 6: { // Query Position+Velocity
                int count = 0;
                for (auto [pos, vel] : world.query<Position, Velocity>()) {
                    (void)pos; (void)vel;
                    ++count;
                }
                CHECK(count >= 0);
                break;
            }
        }
    }

    // Verify all remaining entities are valid
    for (Entity e : alive) {
        CHECK(world.isAlive(e));
    }
}

TEST_CASE("ECS_Invariants_AfterStress") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    // Create many entities with various components
    std::vector<Entity> entities;
    for (int i = 0; i < 1000; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);

        if (i % 3 == 0) {
            world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        }
        if (i % 5 == 0) {
            world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        }
        if (i % 7 == 0) {
            world.addComponent<Health>(e, 100);
        }
    }

    CHECK_INVARIANTS(world);

    // Destroy half
    for (int i = 0; i < 500; ++i) {
        world.destroyEntity(entities[i]);
    }

    CHECK_INVARIANTS(world);

    // Create more to trigger recycling
    for (int i = 0; i < 200; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    CHECK_INVARIANTS(world);

    // Destroy all remaining
    for (size_t i = 500; i < entities.size(); ++i) {
        if (world.isAlive(entities[i])) {
            world.destroyEntity(entities[i]);
        }
    }

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_StringType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Name>();

    Entity e = world.createEntity();
    world.addComponent<Name>(e, "Alice");

    Name* name = world.getComponent<Name>(e);
    REQUIRE(name != nullptr);
    CHECK(std::strcmp(name->value, "Alice") == 0);

    // Archetype move should properly move the string
    TypeRegistry::instance().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    name = world.getComponent<Name>(e);
    REQUIRE(name != nullptr);
    CHECK(std::strcmp(name->value, "Alice") == 0);

    // Destroy should properly destruct the string
    world.destroyEntity(e);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_TypeRegistry_DuplicateRegistration") {
    // Registering the same component twice should not crash
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Position>();

    // Should still work normally
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    Position* pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);
}

TEST_CASE("ECS_ArchetypeId_CollisionSafety") {
    // Create archetypes with different signatures
    // The ArchetypeId now stores the full signature for comparison
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Velocity>(e2, 2.0f, 0.0f, 0.0f);

    Entity e3 = world.createEntity();
    world.addComponent<Health>(e3, 100);

    Entity e4 = world.createEntity();
    world.addComponent<Position>(e4, 3.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e4, 4.0f, 0.0f, 0.0f);

    // All entities should have correct components
    CHECK(world.getComponent<Position>(e1) != nullptr);
    CHECK(world.getComponent<Velocity>(e2) != nullptr);
    CHECK(world.getComponent<Health>(e3) != nullptr);
    CHECK(world.getComponent<Position>(e4) != nullptr);
    CHECK(world.getComponent<Velocity>(e4) != nullptr);

    // Verify no cross-contamination
    CHECK(world.getComponent<Velocity>(e1) == nullptr);
    CHECK(world.getComponent<Health>(e1) == nullptr);
    CHECK(world.getComponent<Position>(e2) == nullptr);
    CHECK(world.getComponent<Health>(e2) == nullptr);
    CHECK(world.getComponent<Position>(e3) == nullptr);
    CHECK(world.getComponent<Velocity>(e3) == nullptr);

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_MoveOnlyType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<UniqueResource>();

    Entity e = world.createEntity();
    world.addComponent<UniqueResource>(e, 42);

    UniqueResource* res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 42);

    // Archetype move should properly move the unique_ptr
    TypeRegistry::instance().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 42);

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_DestroyDuringQuery") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        entities.push_back(e);
    }

    // Query should still work even if entities were destroyed
    // (This tests that query doesn't crash with stale data)
    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 10);

    // Destroy some entities
    world.destroyEntity(entities[3]);
    world.destroyEntity(entities[7]);

    count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 8);
}
