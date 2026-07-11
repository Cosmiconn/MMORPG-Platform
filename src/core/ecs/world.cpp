#include "core/profiling/seed_assert.h"
#include "core/ecs/world.h"
#include "core/profiling/seed_assert.h"
#include "core/profiling/tracy_seed.h"

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
        version = entityVersion(m_entities[index].entity);
        m_nextFree = m_entities[index].nextFree;
    } else {
        index = static_cast<uint32_t>(m_entities.size());
        version = 1;
        m_entities.push_back({makeEntity(index, version), false, INVALID_ENTITY});
        m_records.push_back({ArchetypeId{0}, 0});
    }

    m_entities[index].alive = true;
    m_entities[index].entity = makeEntity(index, version);
    ++m_aliveCount;

    m_records[index] = {ArchetypeId{0}, 0};

    return m_entities[index].entity;
}

void World::destroyEntity(Entity e) {
    SEED_ZONE("World::destroyEntity");
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

Archetype* World::findOrCreateArchetype(const std::vector<ComponentType>& types) {
    ArchetypeId id = makeArchetypeId(types);
    auto it = m_archetypes.find(id.hash);
    if (it != m_archetypes.end()) {
        return it->second.get();
    }

    SEED_ZONE("World::createArchetype");

    std::vector<std::unique_ptr<IComponentArray>> columns;
    columns.reserve(types.size());

    const TypeRegistry& registry = TypeRegistry::instance();
    for (ComponentType ct : types) {
        auto col = registry.createArray(ct, m_allocator);
        SEED_ASSERT(col != nullptr, "Component type not registered in TypeRegistry");
        columns.push_back(std::move(col));
    }

    auto arch = std::make_unique<Archetype>(
        id, types, std::move(columns), m_allocator);

    Archetype* ptr = arch.get();
    m_archetypes[id.hash] = std::move(arch);
    return ptr;
}

Archetype* World::getArchetype(ArchetypeId id) {
    auto it = m_archetypes.find(id.hash);
    return (it != m_archetypes.end()) ? it->second.get() : nullptr;
}

const Archetype* World::getArchetype(ArchetypeId id) const {
    auto it = m_archetypes.find(id.hash);
    return (it != m_archetypes.end()) ? it->second.get() : nullptr;
}

void World::moveEntity(Entity e, Archetype* oldArch, size_t oldIndex,
                       Archetype* newArch,
                       const std::vector<std::pair<ComponentType, const void*>>& preserved) {
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

} // namespace seed::ecs
