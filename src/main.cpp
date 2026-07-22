#include "core/memory/memory_system.h"
#include "core/profiling/tracy_seed.h"
#include "core/ecs/world.h"
#include "core/ecs/type_registry.h"
#include "core/serialize/snapshot.h"
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace seed::memory;
using namespace seed::ecs;

struct Position {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};
SEED_REGISTER_COMPONENT_WITH_ID(Position, 1);

struct Velocity {
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
};
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2);

int main() {
    SEED_ZONE("main");

    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    ArenaAllocator frameArena(&blockAlloc);

    g_blockAllocator = &blockAlloc;
    g_memoryTracker  = &tracker;
    g_frameArena     = &frameArena;

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e1, 0.1f, 0.2f, 0.3f);

    auto e2 = world.createEntity();
    world.addComponent<Position>(e2, 4.0f, 5.0f, 6.0f);

    auto snap = seed::serialize::Snapshot::capture(world);
    const auto& data = snap.data();

#ifdef _WIN32
    // GAP-FIX (2026-07-22, P0-2): stdout ist auf Windows per Default im
    // Text-Modus - ein 0x0A-Byte im rohen Snapshot wuerde sonst von der CRT
    // zu 0x0D 0x0A verfaelscht und einen Byte-fuer-Byte-Vergleich mit dem
    // Linux-Snapshot im cross-platform-compare-Job sabotieren.
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Write raw binary snapshot to stdout for cross-platform byte check
    std::cout.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    SEED_FRAME_MARK();
    return 0;
}
