#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/jobs/job_system.h"
#include "core/memory/memory_system.h"
#include "core/memory/memory_tracker.h"
#include "core/profiling/frame_timer.h"
#include "core/profiling/crash_handler.h"
#include "core/log/log.h"
#include "core/profiling/seed_assert.h"
#include <chrono>
#include <iostream>
#include <csignal>
#include <random>
#include <vector>

using namespace seed;

static std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

struct Position { float x, y, z; };
SEED_REGISTER_COMPONENT_WITH_ID(Position, 100);

struct Velocity { float vx, vy, vz; };
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 101);

struct Health { int32_t hp; int32_t maxHp; };
SEED_REGISTER_COMPONENT_WITH_ID(Health, 102);

int main(int argc, char** argv) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    CrashHandler::install();
    log::LogSystem::instance().initialize("logs/stress_test.log");

    const auto duration = std::chrono::hours(24);
    const auto start = std::chrono::steady_clock::now();

    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    ecs::World world(&blockAlloc);
    jobs::JobSystem js({.numWorkers = 8});
    FrameTimer frameTimer;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    size_t frameCount = 0;
    size_t peakMemory = 0;
    size_t snapshotCount = 0;
    size_t totalDeltaSize = 0;

    SEED_LOG_INFO("Stress test started", duration_hours=24);

    while (g_running && std::chrono::steady_clock::now() - start < duration) {
        frameTimer.beginFrame();

        // === Phase 1: Entity Churn (create / modify / destroy) ===
        for (int i = 0; i < 50; ++i) {
            auto e = world.createEntity();
            world.addComponent<Position>(e, dist(rng), dist(rng), dist(rng));
            world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
            if (i % 3 == 0) {
                world.addComponent<Health>(e, 100, 100);
            }
        }

        // Movement system
        for (auto [pos, vel] : world.query<Position, Velocity>()) {
            pos->x += vel->vx * 0.016f;
            pos->y += vel->vy * 0.016f;
            pos->z += vel->vz * 0.016f;
        }

        // Destroy random entities
        std::vector<seed::ecs::Entity> toDestroy;
        for (auto [pos] : world.query<Position>()) {
            if (rng() % 1000 < 30) { // ~3% destroy rate per frame
                toDestroy.push_back(pos.entity);
            }
        }
        for (auto e : toDestroy) {
            if (world.isAlive(e)) world.destroyEntity(e);
        }

        // === Phase 2: Snapshot Roundtrip ===
        if (frameCount % 60 == 0) { // Every ~1 second @ 60 FPS
            auto snap1 = serialize::Snapshot::capture(world);
            auto data = snap1.serialize();

            // Deserialize roundtrip
            ecs::World world2(&blockAlloc);
            auto snap2 = serialize::Snapshot::deserialize(data);
            snap2.apply(world2);

            SEED_ASSERT(world2.entityCount() == world.entityCount(),
                "Snapshot roundtrip entity count mismatch");

            // Delta test: modify 1% and compute delta
            for (auto [pos] : world.query<Position>()) {
                if (rng() % 100 == 0) pos->x += 1.0f;
            }
            auto snap3 = serialize::Snapshot::capture(world);
            auto delta = snap3.computeDelta(snap1);
            totalDeltaSize += delta.size();
            ++snapshotCount;
        }

        // === Phase 3: Job-System Burn ===
        std::atomic<size_t> jobCounter{0};
        js.parallelFor(1000, [&jobCounter](size_t i) {
            volatile double x = static_cast<double>(i) * 3.14159;
            (void)x;
            ++jobCounter;
        });
        js.waitForAll();
        SEED_ASSERT(jobCounter == 1000, "Job system lost tasks");

        frameTimer.endFrame();

        // === Monitoring ===
        size_t currentMem = tracker.totalUsed();
        if (currentMem > peakMemory) peakMemory = currentMem;

        if (frameCount % 3600 == 0) { // Every minute
            auto elapsed = std::chrono::steady_clock::now() - start;
            double hours = std::chrono::duration<double>(elapsed).count() / 3600.0;

            SEED_LOG_INFO("Stress progress",
                hours=fmt::format("{:.2f}", hours),
                frame=frameCount,
                memory_mb=currentMem / (1024*1024),
                peak_mb=peakMemory / (1024*1024),
                fps=fmt::format("{:.1f}", frameTimer.averageFps()),
                entities=world.entityCount(),
                snapshots=snapshotCount);
        }

        // Budget alarm
        if (frameTimer.isOverBudget(20.0f)) {
            SEED_LOG_WARN("Frame over budget",
                delta_ms=fmt::format("{:.2f}", frameTimer.getCurrentStats().deltaTime),
                entities=world.entityCount());
        }

        ++frameCount;
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    double hours = std::chrono::duration<double>(elapsed).count() / 3600.0;
    double avgDeltaSize = snapshotCount > 0 ? static_cast<double>(totalDeltaSize) / snapshotCount : 0.0;

    SEED_LOG_INFO("Stress test completed",
        hours=fmt::format("{:.2f}", hours),
        frames=frameCount,
        peak_memory_mb=peakMemory / (1024*1024),
        avg_delta_kb=fmt::format("{:.1f}", avgDeltaSize / 1024.0));

    // Gate 0 checks
    bool passed = true;
    if (peakMemory > 200 * 1024 * 1024) {
        SEED_LOG_ERROR("Memory budget exceeded", peak_mb=peakMemory / (1024*1024), budget_mb=200);
        passed = false;
    }
    if (frameCount > 0 && hours > 0) {
        double avgFps = frameCount / (hours * 3600.0);
        if (avgFps < 30.0) {
            SEED_LOG_ERROR("Performance degraded", avg_fps=fmt::format("{:.1f}", avgFps), budget_fps=30);
            passed = false;
        }
    }

    return passed ? 0 : 1;
}
