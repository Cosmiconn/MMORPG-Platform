#include <doctest/doctest.h>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"
#include "core/diagnostics/diagnostics_manager.h"
#include "core/diagnostics/event_timeline.h"
#include <random>
#include <fmt/format.h>

using namespace seed::ecs;
using namespace seed::memory;
using namespace seed::diagnostics;

struct FuzzPosition {
    float x, y, z;
    FuzzPosition() : x(0), y(0), z(0) {}
    FuzzPosition(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct FuzzVelocity {
    float dx, dy, dz;
    FuzzVelocity() : dx(0), dy(0), dz(0) {}
    FuzzVelocity(float dx_, float dy_, float dz_) : dx(dx_), dy(dy_), dz(dz_) {}
};

struct FuzzHealth {
    int hp;
    FuzzHealth() : hp(100) {}
    explicit FuzzHealth(int hp_) : hp(hp_) {}
};

struct FuzzArmor {
    int value;
    FuzzArmor() : value(0) {}
    explicit FuzzArmor(int v) : value(v) {}
};

struct FuzzName {
    std::string value;
    FuzzName() = default;
    explicit FuzzName(std::string v) : value(std::move(v)) {}
};

SEED_REGISTER_COMPONENT(FuzzPosition);
SEED_REGISTER_COMPONENT(FuzzVelocity);
SEED_REGISTER_COMPONENT(FuzzHealth);
SEED_REGISTER_COMPONENT(FuzzArmor);
SEED_REGISTER_COMPONENT(FuzzName);

// ---------------------------------------------------------------------------
// Fuzz Engine with Deterministic Replay
// ---------------------------------------------------------------------------
class FuzzEngine {
public:
    explicit FuzzEngine(uint64_t seed) : m_rng(seed), m_seed(seed) {}

    void run(World& world, int operations) {
        auto& diag = DiagnosticsManager::instance();

        for (int i = 0; i < operations; ++i) {
            int op = m_rng() % 6;

            switch (op) {
                case 0: fuzzCreateEntity(world); break;
                case 1: fuzzAddComponent(world); break;
                case 2: fuzzRemoveComponent(world); break;
                case 3: fuzzDestroyEntity(world); break;
                case 4: fuzzQuery(world); break;
                case 5: fuzzArchetypeMove(world); break;
            }

            // Periodic validation
            if (i % 100 == 0) {
                if (!world.validateInvariants()) {
                    diag.health().setScore(HealthScore::Module::ECS, 0);
                    SEED_DIAG_EVENT(EventType::InvariantFail, INVALID_ENTITY, 0, 0, 0,
                        fmt::format("Invariant failed at op {} with seed {}", i, m_seed).c_str(),
                        __FILE__, __LINE__);

                    // Generate replay info
                    fmt::print("\n=== FUZZ FAILURE ===\n");
                    fmt::print("REPRODUCE WITH SEED: {}\n", m_seed);
                    fmt::print("Operation: {}\n", i);
                    fmt::print("===================\n\n");

                    diag.snapshot(world, "fuzz-invariant-failure");
                    CHECK(false); // Force test failure
                    return;
                }
            }
        }
    }

    uint64_t seed() const { return m_seed; }

private:
    std::mt19937 m_rng;
    uint64_t m_seed;
    std::vector<Entity> m_alive;

    void fuzzCreateEntity(World& world) {
        Entity e = world.createEntity();
        m_alive.push_back(e);

        // Random components
        if (m_rng() % 2 == 0) world.addComponent<FuzzPosition>(e, 
            float(m_rng() % 100), float(m_rng() % 100), float(m_rng() % 100));
        if (m_rng() % 3 == 0) world.addComponent<FuzzVelocity>(e,
            float(m_rng() % 10), float(m_rng() % 10), float(m_rng() % 10));
        if (m_rng() % 5 == 0) world.addComponent<FuzzHealth>(e, int(m_rng() % 100));
        if (m_rng() % 7 == 0) world.addComponent<FuzzArmor>(e, int(m_rng() % 50));
        if (m_rng() % 11 == 0) world.addComponent<FuzzName>(e, 
            fmt::format("Entity_{}", m_rng()));
    }

    void fuzzAddComponent(World& world) {
        if (m_alive.empty()) return;
        size_t idx = m_rng() % m_alive.size();
        Entity e = m_alive[idx];
        if (!world.isAlive(e)) return;

        int comp = m_rng() % 5;
        switch (comp) {
            case 0: world.addComponent<FuzzPosition>(e, 1.0f, 2.0f, 3.0f); break;
            case 1: world.addComponent<FuzzVelocity>(e, 1.0f, 0.0f, 0.0f); break;
            case 2: world.addComponent<FuzzHealth>(e, 100); break;
            case 3: world.addComponent<FuzzArmor>(e, 50); break;
            case 4: world.addComponent<FuzzName>(e, "fuzz"); break;
        }
    }

    void fuzzRemoveComponent(World& world) {
        if (m_alive.empty()) return;
        size_t idx = m_rng() % m_alive.size();
        Entity e = m_alive[idx];
        if (!world.isAlive(e)) return;

        int comp = m_rng() % 5;
        switch (comp) {
            case 0: world.removeComponent<FuzzPosition>(e); break;
            case 1: world.removeComponent<FuzzVelocity>(e); break;
            case 2: world.removeComponent<FuzzHealth>(e); break;
            case 3: world.removeComponent<FuzzArmor>(e); break;
            case 4: world.removeComponent<FuzzName>(e); break;
        }
    }

    void fuzzDestroyEntity(World& world) {
        if (m_alive.empty()) return;
        size_t idx = m_rng() % m_alive.size();
        Entity e = m_alive[idx];
        if (world.isAlive(e)) {
            world.destroyEntity(e);
        }
        m_alive.erase(m_alive.begin() + idx);
    }

    void fuzzQuery(World& world) {
        int qtype = m_rng() % 3;
        switch (qtype) {
            case 0: {
                for (auto [pos] : world.query<FuzzPosition>()) { (void)pos; }
                break;
            }
            case 1: {
                for (auto [pos, vel] : world.query<FuzzPosition, FuzzVelocity>()) { 
                    (void)pos; (void)vel; 
                }
                break;
            }
            case 2: {
                for (auto [pos, vel, hp] : world.query<FuzzPosition, FuzzVelocity, FuzzHealth>()) {
                    (void)pos; (void)vel; (void)hp;
                }
                break;
            }
        }
    }

    void fuzzArchetypeMove(World& world) {
        if (m_alive.empty()) return;
        size_t idx = m_rng() % m_alive.size();
        Entity e = m_alive[idx];
        if (!world.isAlive(e)) return;

        // Add all components to force maximum archetype complexity
        world.addComponent<FuzzPosition>(e, 1.0f, 2.0f, 3.0f);
        world.addComponent<FuzzVelocity>(e, 1.0f, 0.0f, 0.0f);
        world.addComponent<FuzzHealth>(e, 100);
        world.addComponent<FuzzArmor>(e, 50);
        world.addComponent<FuzzName>(e, "max_complexity");

        // Then remove some to force moves back down
        world.removeComponent<FuzzArmor>(e);
        world.removeComponent<FuzzName>(e);
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("Fuzz_ECS_Deterministic_1000ops") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<FuzzPosition>();
    world.typeRegistry().registerComponent<FuzzVelocity>();
    world.typeRegistry().registerComponent<FuzzHealth>();
    world.typeRegistry().registerComponent<FuzzArmor>();
    world.typeRegistry().registerComponent<FuzzName>();

    uint64_t seed = 42; // Fixed seed for reproducibility
    FuzzEngine fuzz(seed);
    fuzz.run(world, 1000);

    CHECK(world.validateInvariants());

    fmt::print("\n=== Fuzz Report (seed={}) ===\n", seed);
    fmt::print("{}", diag.timeline().performanceReport());
    fmt::print("{}", diag.fullReport());
}

TEST_CASE("Fuzz_ECS_RandomSeed_10000ops") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<FuzzPosition>();
    world.typeRegistry().registerComponent<FuzzVelocity>();
    world.typeRegistry().registerComponent<FuzzHealth>();
    world.typeRegistry().registerComponent<FuzzArmor>();
    world.typeRegistry().registerComponent<FuzzName>();

    // Use time-based seed, but print it for reproduction
    uint64_t seed = std::random_device{}();
    fmt::print("\n[FUZZ] Using random seed: {}\n", seed);

    FuzzEngine fuzz(seed);
    fuzz.run(world, 10000);

    CHECK(world.validateInvariants());

    // On failure, the seed is printed by the FuzzEngine
    fmt::print("[FUZZ] Completed with seed: {}\n", seed);
}

TEST_CASE("Fuzz_ECS_MoveOnlyStress") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<FuzzPosition>();
    world.typeRegistry().registerComponent<FuzzName>(); // std::string = move-only relevant

    uint64_t seed = 12345;
    FuzzEngine fuzz(seed);
    fuzz.run(world, 5000);

    CHECK(world.validateInvariants());

    // Verify no move-only errors
    auto moveErrors = diag.timeline().getEventsByType(EventType::MoveOnlyError);
    auto asserts = diag.timeline().getEventsByType(EventType::AssertionFail);
    CHECK(moveErrors.empty());
    CHECK(asserts.empty());
}

TEST_CASE("Fuzz_ECS_HealthScoreTracking") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    diag.timeline().clear();

    world.typeRegistry().registerComponent<FuzzPosition>();

    uint64_t seed = 99999;
    FuzzEngine fuzz(seed);
    fuzz.run(world, 2000);

    // Health score should have been updated during operations
    auto& health = diag.health();
    fmt::print("\n=== Health Score after Fuzz ===\n");
    fmt::print("{}", health.report());

    // Should still be somewhat healthy (not 0) if no critical errors
    CHECK(health.getScore(HealthScore::Module::ECS) > 0);
}
