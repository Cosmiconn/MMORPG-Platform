#include "core/ecs/world.h"
#include "core/ecs/archetype_manager.h"
#include "core/profiling/seed_assert.h"
#include "core/profiling/tracy_seed.h"
#include <fmt/format.h>

namespace seed::ecs {

World::World(seed::memory::Allocator* allocator)
    : m_allocator(allocator)
    , m_nextFree(INVALID_ENTITY)
    , m_aliveCount(0)
    , m_nextVersion(1)
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
    uint32_t index;
    uint8_t version;

    if (m_nextFree != INVALID_ENTITY) {
        index = m_nextFree;
        // Increment version on recycle to invalidate old handles
        version = entityVersion(m_entities[index].entity) + 1;
        if (version == 0) version = 1; // wrap-around guard
        m_nextFree = m_entities[index].nextFree;
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

    return m_entities[index].entity;
}

void World::destroyEntity(Entity e) {
    SEED_ZONE("World::destroyEntity");
    SEED_ASSERT(e != INVALID_ENTITY, "destroyEntity called with invalid entity");
    if (!isAlive(e)) return;

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

void World::moveEntity(Entity e, Archetype* oldArch, size_t oldIndex,
                       Archetype* newArch,
                       const std::vector<std::pair<ComponentType, const void*>>& preserved) {
    SEED_ASSERT(oldArch != nullptr, "moveEntity called with null oldArch");
    SEED_ASSERT(newArch != nullptr, "moveEntity called with null newArch");
    SEED_ASSERT(isAlive(e), "moveEntity called on dead entity");
    oldArch->removeEntityByIndex(oldIndex);
    if (oldIndex < oldArch->size()) {
        Entity moved = oldArch->entityAt(oldIndex);
        m_records[entityIndex(moved)].index = static_cast<uint32_t>(oldIndex);
    }

    size_t newIndex = newArch->addEntity(e);

    for (const auto& [ct, data] : preserved) {
        newArch->setComponent(newIndex, ct, data);
    }

    m_records[entityIndex(e)] = {newArch->id(), static_cast<uint32_t>(newIndex)};
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



void World::dump() const {
    fmt::print("=== World Dump ===\n");
    fmt::print("Alive entities: {}\n", m_aliveCount);
    fmt::print("Total slots: {}\n", m_entities.size());
    fmt::print("Archetypes: {}\n", m_archetypeManager->archetypeCount());
    for (const auto& [id, arch] : *m_archetypeManager) {
        fmt::print("  Archetype {:08x}: {} entities, {} components\n",
                   id.hash, arch->size(), arch->componentTypes().size());
    }
    fmt::print("==================\n");
}

bool World::validateInvariants() const {
    size_t alive = 0;
    for (const auto& slot : m_entities) {
        if (slot.alive) ++alive;
    }
    if (alive != m_aliveCount) {
        fmt::print("INVARIANT VIOLATION: alive count mismatch ({} vs {})\n", alive, m_aliveCount);
        return false;
    }

    for (size_t i = 0; i < m_entities.size(); ++i) {
        if (m_entities[i].alive) {
            const auto& rec = m_records[i];
            auto* arch = getArchetype(rec.archetypeId);
            if (!arch) {
                fmt::print("INVARIANT VIOLATION: entity {} has invalid archetype\n", i);
                return false;
            }
            if (rec.index >= arch->size()) {
                fmt::print("INVARIANT VIOLATION: entity {} index {} out of range {}\n",
                           i, rec.index, arch->size());
                return false;
            }
            if (arch->entityAt(rec.index) != m_entities[i].entity) {
                fmt::print("INVARIANT VIOLATION: entity {} mismatch at archetype index {}\n",
                           i, rec.index);
                return false;
            }
        }
    }

    return true;
}
} // namespace seed::ecs
