#include <doctest/doctest.h>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"

using namespace seed::ecs;
using namespace seed::memory;

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
};

struct Name {
    char value[32] = {};
};

SEED_REGISTER_COMPONENT(Position, 1);
SEED_REGISTER_COMPONENT(Velocity, 2);
SEED_REGISTER_COMPONENT(Health, 3);
SEED_REGISTER_COMPONENT(Name, 4);

TEST_CASE("ECS_Entity_CreateDestroy") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

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

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    for (int i = 0; i < 5; ++i) {
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

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    auto result = world.query<Position, Velocity>();
    CHECK(result.empty());
    CHECK(result.count() == 0);
}
