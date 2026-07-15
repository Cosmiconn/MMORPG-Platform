#include "core/ecs/world.h"
#include "core/ecs/archetype_manager.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/ecs_validator.h"
#include "core/profiling/seed_assert.h"
#include "core/profiling/tracy_seed.h"
#include <fmt/format.h>
#include <chrono>

namespace seed::ecs {

World::World(seed::memory::Allocator* allocator)
    : m_allocator(allocator)
    , m_nextFree(INVALID_ENTITY)
    , m_aliveCount(0)
    , m_nextVersion(1)
    , m_archetypeManager(std::make_unique<ArchetypeManager>(allocator, &m_typeRegistry))
{
    SEED_ZONE("World::ctor");
    m_entities.reserve(1024);
    m_records.reserve(1024);
}

World::~World() {
    SEED_ZONE("World::dtor");
    for (auto it = m_systems.rbegin(); it != m_systems.rend(); ++it) {
        (*it)->onShutdown(this);
    }
}

Entity World::createEntity() {
    SEED_ZONE("World::createEntity");
    auto start = std::chrono::steady_clock::now();

    uint32_t index;
    uint8_t version;

    if (m_nextFree != INVALID_ENTITY) {
        index = m_nextFree;
        version = entityVersion(m_entities[index].entity) + 1;
        if (version == 0) version = 1;
        m_nextFree = m_entities[index].nextFree;

        SEED_DIAG_EVENT(seed::diagnostics::EventType::EntityRecycle,
            makeEntity(index, version), 0, 0, 0,
            "Entity recycled with new version", __FILE__, __LINE__);
    } else {
        index = static_cast<uint32_t>(m_entities.size());
        version = 1;
        m_entities.push_back({makeEntity(index, version), false, INVALID_ENTITY});
        m_records.push_back({ArchetypeId{0, {}}, 0});
    }

    m_entities[index].alive = true;
    m_entities[index].entity = makeEntity(index, version);
    ++m_aliveCount;
    m_records[index] = {ArchetypeId{0, {}}, 0};

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

    SEED_DIAG_EVENT_PERF(seed::diagnostics::EventType::EntityCreate,
        m_entities[index].entity, 0, 0, 0,
        "createEntity complete", __FILE__, __LINE__, static_cast<uint64_t>(ns));

    return m_entities[index].entity;
}

void World::destroyEntity(Entity e) {
    SEED_ZONE("World::destroyEntity");
    auto start = std::chrono::steady_clock::now();

    SEED_DIAG_EVENT(seed::diagnostics::EventType::EntityDestroy,
        e, 0, 0, 0, "destroyEntity start", __FILE__, __LINE__);

    SEED_ASSERT(e != INVALID_ENTITY, "destroyEntity called with invalid entity");
    if (!isAlive(e)) {
        SEED_DIAG_EVENT(seed::diagnostics::EventType::AssertionFail,
            e, 0, 0, 0, "destroyEntity called on dead entity", __FILE__, __LINE__);
        return;
    }

    uint32_t idx = entityIndex(e);

    const EntityRecord& rec = m_records[idx];
    if (rec.archetypeId.hash != 0) {
        Archetype* arch = getArchetype(rec.archetypeId);
        if (arch) {
            arch->removeEntityByIndex(rec.index);
            if (rec.index < arch->size()) {
                Entity moved = arch->entityAt(rec.index);
                m_records[entityIndex(moved)].index = static_cast<uint32_t>(rec.index);
            }
        }
    }

    m_entities[idx].alive = false;
    m_entities[idx].nextFree = m_nextFree;
    m_nextFree = idx;
    --m_aliveCount;
    m_records[idx] = {ArchetypeId{0, {}}, 0};

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

    SEED_DIAG_EVENT_PERF(seed::diagnostics::EventType::EntityDestroy,
        e, 0, 0, 0, "destroyEntity complete", __FILE__, __LINE__, static_cast<uint64_t>(ns));
}

bool World::isAlive(Entity e) const {
    uint32_t idx = entityIndex(e);
    if (idx >= m_entities.size()) return false;
    return m_entities[idx].alive && entityVersion(m_entities[idx].entity) == entityVersion(e);
}

void World::registerSystem(std::unique_ptr<System> system) {
    SEED_ZONE("World::registerSystem");
    system->onInit(this);
    m_systems.push_back(std::move(system));
    std::sort(m_systems.begin(), m_systems.end(),
        [](const auto& a, const auto& b) {
            return a->priority() < b->priority();
        });
}

void World::update(float deltaTime) {
    SEED_ZONE("World::update");
    for (auto& sys : m_systems) {
        SEED_ZONE("System::update");
        sys->onUpdate(this, deltaTime);
    }
}

Archetype* World::getArchetype(ArchetypeId id) {
    return m_archetypeManager->getArchetype(id);
}

const Archetype* World::getArchetype(ArchetypeId id) const {
    return m_archetypeManager->getArchetype(id);
}

void World::moveEntity(Entity e, Archetype* oldArch, size_t oldIndex, Archetype* newArch) {
    auto start = std::chrono::steady_clock::now();

    SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentMove,
        e, oldArch ? oldArch->id().hash : 0,
        newArch ? newArch->id().hash : 0, 0,
        "moveEntity start", __FILE__, __LINE__);

    SEED_ASSERT(oldArch != nullptr, "moveEntity called with null oldArch");
    SEED_ASSERT(newArch != nullptr, "moveEntity called with null newArch");
    SEED_ASSERT(isAlive(e), "moveEntity called on dead entity");
    SEED_ASSERT(oldIndex < oldArch->size(), "moveEntity oldIndex out of bounds");
    SEED_ASSERT(oldArch->entityAt(oldIndex) == e, "moveEntity entity mismatch at oldArch");

    size_t newIndex = newArch->addEntity(e);

    for (ComponentType ct : newArch->componentTypes()) {
        if (oldArch->hasComponent(ct)) {
            IComponentArray* srcCol = oldArch->getColumn(ct);
            newArch->moveComponent(newIndex, ct, srcCol, oldIndex);
        }
    }

    oldArch->removeEntityByIndex(oldIndex);
    if (oldIndex < oldArch->size()) {
        Entity moved = oldArch->entityAt(oldIndex);
        m_records[entityIndex(moved)].index = static_cast<uint32_t>(oldIndex);
    }

    m_records[entityIndex(e)] = {newArch->id(), static_cast<uint32_t>(newIndex)};

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

    SEED_DIAG_EVENT_PERF(seed::diagnostics::EventType::ComponentMove,
        e, newArch->id().hash, 0, static_cast<uint32_t>(newIndex),
        "moveEntity complete", __FILE__, __LINE__, static_cast<uint64_t>(ns));
}

void* World::getComponentRaw(Entity e, ComponentType type) {
    if (!isAlive(e)) return nullptr;
    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* arch = getArchetype(rec.archetypeId);
    if (!arch) return nullptr;
    return arch->getComponent(rec.index, type);
}

void World::setComponentRaw(Entity e, ComponentType type, const void* data) {
    if (!isAlive(e)) return;
    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* arch = getArchetype(rec.archetypeId);
    if (arch) {
        arch->setComponent(rec.index, type, data);
    }
}

std::string World::dumpToString() const {
    std::string out;
    out += fmt::format("Alive entities: {}\n", m_aliveCount);
    out += fmt::format("Total slots: {}\n", m_entities.size());
    out += fmt::format("Archetypes: {}\n", m_archetypeManager->archetypeCount());
    for (const auto& [id, arch] : *m_archetypeManager) {
        out += fmt::format("  Archetype {:08x}: {} entities, {} components\n",
                   id.hash, arch->size(), arch->componentTypes().size());
    }
    return out;
}

void World::dump() const {
    fmt::print("=== World Dump ===\n");
    fmt::print("{}", dumpToString());
    fmt::print("==================\n");
}

bool World::validateInvariants() const {
    SEED_DIAG_EVENT(seed::diagnostics::EventType::WorldValidate, INVALID_ENTITY, 0, 0, 0,
                    "validateInvariants start", __FILE__, __LINE__);
    size_t alive = 0;
    for (const auto& slot : m_entities) {
        if (slot.alive) ++alive;
    }
    if (alive != m_aliveCount) {
        fmt::print("INVARIANT VIOLATION: alive count mismatch ({} vs {})\n", alive, m_aliveCount);
        SEED_DIAG_EVENT(seed::diagnostics::EventType::InvariantFail, INVALID_ENTITY, 0, 0, 0,
                        "alive count mismatch", __FILE__, __LINE__);
        return false;
    }

    for (size_t i = 0; i < m_entities.size(); ++i) {
        if (m_entities[i].alive) {
            const auto& rec = m_records[i];
            if (rec.archetypeId.hash == 0) {
                continue;
            }
            auto* arch = getArchetype(rec.archetypeId);
            if (!arch) {
                fmt::print("INVARIANT VIOLATION: entity {} has invalid archetype\n", i);
                SEED_DIAG_EVENT(seed::diagnostics::EventType::InvariantFail, m_entities[i].entity, 0, 0, 0,
                                "invalid archetype", __FILE__, __LINE__);
                return false;
            }
            if (rec.index >= arch->size()) {
                fmt::print("INVARIANT VIOLATION: entity {} index {} out of range {}\n",
                           i, rec.index, arch->size());
                SEED_DIAG_EVENT(seed::diagnostics::EventType::InvariantFail, m_entities[i].entity,
                                rec.archetypeId.hash, 0, rec.index,
                                "index out of range", __FILE__, __LINE__);
                return false;
            }
            if (arch->entityAt(rec.index) != m_entities[i].entity) {
                fmt::print("INVARIANT VIOLATION: entity {} mismatch at archetype index {}\n",
                           i, rec.index);
                SEED_DIAG_EVENT(seed::diagnostics::EventType::InvariantFail, m_entities[i].entity,
                                rec.archetypeId.hash, 0, rec.index,
                                "entity mismatch", __FILE__, __LINE__);
                return false;
            }
        }
    }

    return true;
}
} // namespace seed::ecs
