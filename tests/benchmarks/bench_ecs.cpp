#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"
#include <chrono>
#include <iostream>

using namespace seed::ecs;
using namespace seed::memory;

struct Position {
    float x, y, z;
    Position() = default;
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Velocity {
    float vx, vy, vz;
    Velocity() = default;
    Velocity(float vx_, float vy_, float vz_) : vx(vx_), vy(vy_), vz(vz_) {}
};

struct Health {
    int hp = 100;
    int maxHp = 100;
    Health() = default;
    explicit Health(int hp_) : hp(hp_), maxHp(hp_) {}
};

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)
SEED_REGISTER_COMPONENT_WITH_ID(Health, 3)

int main() {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    world.typeRegistry().registerComponent<Position>();
    world.typeRegistry().registerComponent<Velocity>();
    world.typeRegistry().registerComponent<Health>();

    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < 100'000; ++i) {
            Entity e = world.createEntity();
            world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
            world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
            if (i % 2 == 0) world.addComponent<Health>(e);
        }
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "100k entities created: " << ms << " ms\n";
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        float sum = 0.0f;
        for (int frame = 0; frame < 100; ++frame) {
            for (auto [pos, vel] : world.query<Position, Velocity>()) {
                pos->x += vel->vx * 0.016f;
                sum += pos->x;
            }
        }
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "100 frames query+update: " << ms << " ms (sum=" << sum << ")\n";
    }

    std::cout << "Entity count: " << world.entityCount() << "\n";

    return 0;
}
