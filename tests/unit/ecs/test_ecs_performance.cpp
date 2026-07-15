#include <doctest/doctest.h>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"
#include "core/diagnostics/diagnostics_manager.h"
#include "core/diagnostics/event_timeline.h"
#include <chrono>
#include <fmt/format.h>

using namespace seed::ecs;
using namespace seed::memory;
using namespace seed::diagnostics;

struct PerfPosition {
    float x, y, z;
    PerfPosition() : x(0), y(0), z(0) {}
    PerfPosition(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct PerfVelocity {
    float dx, dy, dz;
    PerfVelocity() : dx(0), dy(0), dz(0) {}
    PerfVelocity(float dx_, float dy_, float dz_) : dx(dx_), dy(dy_), dz(dz_) {}
};

struct PerfHealth {
    int hp;
    PerfHealth() : hp(100) {}
    explicit PerfHealth(int hp_) : hp(hp_) {}
};

struct PerfArmor {
    int value;
    PerfArmor() : value(0) {}
    explicit PerfArmor(int v) : value(v) {}
};

SEED_REGISTER_COMPONENT(PerfPosition);
SEED_REGISTER_COMPONENT(PerfVelocity);
SEED_REGISTER_COMPONENT(PerfHealth);
SEED_REGISTER_COMPONENT(PerfArmor);

struct MovementSystem : System {
    void onUpdate(World* w, float dt) override {
        for (auto [pos, vel] : w->query<PerfPosition, PerfVelocity>()) {
            pos->x += vel->dx * dt;
            pos->y += vel->dy * dt;
            pos->z += vel->dz * dt;
        }
    }
};

TEST_CASE("ECS_100k_Entities_Create") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<PerfPosition>();
    world.typeRegistry().registerComponent<PerfVelocity>();
    world.typeRegistry().registerComponent<PerfHealth>();
    world.typeRegistry().registerComponent<PerfArmor>();

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < 100'000; ++i) {
        Entity e = world.createEntity();
        world.addComponent<PerfPosition>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<PerfVelocity>(e, 1.0f, 0.0f, 0.0f);
        world.addComponent<PerfHealth>(e);
        world.addComponent<PerfArmor>(e);
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CHECK(world.entityCount() == 100'000);

    // Relaxed tolerance for CI runners with diagnostics enabled
    // Debug + ASan + Diagnostics = ~3x slower than release
    CHECK(ms < 10000);  // 10s tolerance for debug CI builds

    // Print performance report
    fmt::print("\n=== 100k Create Performance ===\n");
    fmt::print("Total time: {}ms\n", ms);
    fmt::print("{}", diag.timeline().performanceReport());
    fmt::print("================================\n\n");
}

TEST_CASE("ECS_100k_Entities_Systems_60FPS") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    world.typeRegistry().registerComponent<PerfPosition>();
    world.typeRegistry().registerComponent<PerfVelocity>();
    world.typeRegistry().registerComponent<PerfHealth>();
    world.typeRegistry().registerComponent<PerfArmor>();

    for (size_t i = 0; i < 100'000; ++i) {
        Entity e = world.createEntity();
        world.addComponent<PerfPosition>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<PerfVelocity>(e, 1.0f, 0.0f, 0.0f);
        world.addComponent<PerfHealth>(e);
        world.addComponent<PerfArmor>(e);
    }

    world.registerSystem(std::make_unique<MovementSystem>());

    auto start = std::chrono::high_resolution_clock::now();
    world.update(1.0f / 60.0f);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CHECK(ms < 17); // < 16.67ms for 60 FPS
}

TEST_CASE("ECS_Archetype_Change") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    world.typeRegistry().registerComponent<PerfPosition>();
    world.typeRegistry().registerComponent<PerfVelocity>();
    world.typeRegistry().registerComponent<PerfHealth>();

    std::vector<Entity> entities;
    for (int i = 0; i < 1000; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);
        world.addComponent<PerfPosition>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (Entity e : entities) {
        world.addComponent<PerfVelocity>(e, 1.0f, 0.0f, 0.0f);
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CHECK(ms < 100); // Should be fast

    start = std::chrono::high_resolution_clock::now();
    for (Entity e : entities) {
        world.addComponent<PerfHealth>(e, 100);
    }
    elapsed = std::chrono::high_resolution_clock::now() - start;
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CHECK(ms < 100);
}
