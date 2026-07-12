#pragma once

#include "core/profiling/seed_assert.h"
#include "core/ecs/archetype.h"
#include "core/ecs/archetype_manager.h"
#include "core/ecs/component_array.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/entity.h"
#include "core/ecs/query.h"
#include "core/ecs/system.h"
#include "core/ecs/type_registry.h"
#include "core/memory/allocator.h"
#include "core/profiling/tracy_seed.h"
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace seed::ecs {

// EntityRecord uses direct index lookup (O(1))
// No linear search needed - entity index is encoded in Entity handle
struct EntityRecord {
    ArchetypeId archetypeId;
    uint32_t index;
};

class World {
public:
    explicit World(seed::memory::Allocator* allocator);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity createEntity();
    void destroyEntity(Entity e);
    bool isAlive(Entity e) const;

    size_t entityCount() const noexcept { return m_aliveCount; }

    template<typename T, typename... Args>
    T* addComponent(Entity e, Args&&... args);

    template<typename T>
    T* getComponent(Entity e);

    template<typename T>
    const T* getComponent(Entity e) const;

    template<typename T>
    bool hasComponent(Entity e) const;

    template<typename T>
    void removeComponent(Entity e);

    void registerSystem(std::unique_ptr<System> system);
    void update(float deltaTime);

    template<typename... Components>
    QueryResult<Components...> query();

    void* getComponentRaw(Entity e, ComponentType type);
    void setComponentRaw(Entity e, ComponentType type, const void* data);

private:
    struct EntitySlot {
        Entity entity;
        bool alive;
        uint32_t nextFree;
    };

    seed::memory::Allocator* m_allocator;
    std::vector<EntitySlot> m_entities;
    std::vector<EntityRecord> m_records;
    uint32_t m_nextFree = INVALID_ENTITY;
    uint32_t m_aliveCount = 0;
    uint32_t m_nextVersion = 1;

    std::unique_ptr<ArchetypeManager> m_archetypeManager;
    std::vector<std::unique_ptr<System>> m_systems;

    Archetype* findOrCreateArchetype(const std::vector<ComponentType>& types) {
        return m_archetypeManager->findOrCreateArchetype(types);
    }
    Archetype* getArchetype(ArchetypeId id);
    const Archetype* getArchetype(ArchetypeId id) const;
    void moveEntity(Entity e, Archetype* oldArch, size_t oldIndex,
                    Archetype* newArch,
                    const std::vector<std::pair<ComponentType, const void*>>& preserved);
};

template<typename T, typename... Args>
T* World::addComponent(Entity e, Args&&... args) {
    SEED_ZONE("World::addComponent");
    if (!isAlive(e)) return nullptr;

    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* oldArch = getArchetype(rec.archetypeId);
    ComponentType newType = ComponentTraits<T>::id;

    // Check if component already exists on this entity
    if (oldArch != nullptr && oldArch->hasComponent(newType)) {
        SEED_ASSERT(false, "addComponent called with duplicate component type");
        return nullptr;
    }

    // Entity has no components yet (empty archetype, hash=0)
    if (oldArch == nullptr) {
        std::vector<ComponentType> newTypes = {newType};
        Archetype* newArch = findOrCreateArchetype(newTypes);
        size_t newIndex = newArch->addEntity(e);
        m_records[entityIndex(e)] = {newArch->id(), static_cast<uint32_t>(newIndex)};
        T* newSlot = newArch->getComponent<T>(newIndex);
        new (newSlot) T(std::forward<Args>(args)...);
        return newSlot;
    }

    std::vector<ComponentType> newTypes = oldArch->componentTypes();

    if (std::binary_search(newTypes.begin(), newTypes.end(), newType)) {
        T* slot = oldArch->getComponent<T>(rec.index);
        *slot = T(std::forward<Args>(args)...);
        return slot;
    }

    newTypes.push_back(newType);
    std::sort(newTypes.begin(), newTypes.end());

    std::vector<std::pair<ComponentType, const void*>> preserved;
    preserved.reserve(oldArch->componentTypes().size());
    for (ComponentType ct : oldArch->componentTypes()) {
        preserved.emplace_back(ct, oldArch->getComponent(rec.index, ct));
    }

    Archetype* newArch = findOrCreateArchetype(newTypes);
    moveEntity(e, oldArch, rec.index, newArch, preserved);

    T* newSlot = newArch->getComponent<T>(m_records[entityIndex(e)].index);
    new (newSlot) T(std::forward<Args>(args)...);
    return newSlot;
}

template<typename T>
T* World::getComponent(Entity e) {
    if (!isAlive(e)) return nullptr;
    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* arch = getArchetype(rec.archetypeId);
    if (!arch || !arch->hasComponent(ComponentTraits<T>::id)) return nullptr;
    return arch->getComponent<T>(rec.index);
}

template<typename T>
const T* World::getComponent(Entity e) const {
    return const_cast<World*>(this)->getComponent<T>(e);
}

template<typename T>
bool World::hasComponent(Entity e) const {
    if (!isAlive(e)) return false;
    const EntityRecord& rec = m_records[entityIndex(e)];
    const Archetype* arch = getArchetype(rec.archetypeId);
    return arch && arch->hasComponent(ComponentTraits<T>::id);
}

template<typename T>
void World::removeComponent(Entity e) {
    SEED_ZONE("World::removeComponent");
    if (!isAlive(e)) return;

    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* oldArch = getArchetype(rec.archetypeId);
    if (oldArch == nullptr) return; // Entity has no components

    ComponentType remType = ComponentTraits<T>::id;
    std::vector<ComponentType> newTypes;
    newTypes.reserve(oldArch->componentTypes().size() - 1);

    std::vector<std::pair<ComponentType, const void*>> preserved;
    preserved.reserve(newTypes.capacity());

    for (ComponentType ct : oldArch->componentTypes()) {
        if (ct != remType) {
            newTypes.push_back(ct);
            preserved.emplace_back(ct, oldArch->getComponent(rec.index, ct));
        }
    }

    if (newTypes.size() == oldArch->componentTypes().size()) return;

    if (newTypes.empty()) {
        // Entity now has no components - move to empty archetype
        oldArch->removeEntityByIndex(rec.index);
        if (rec.index < oldArch->size()) {
            Entity moved = oldArch->entityAt(rec.index);
            m_records[entityIndex(moved)].index = static_cast<uint32_t>(rec.index);
        }
        m_records[entityIndex(e)] = {ArchetypeId{0}, 0};
        return;
    }

    Archetype* newArch = findOrCreateArchetype(newTypes);
    moveEntity(e, oldArch, rec.index, newArch, preserved);
}

template<typename... Components>
QueryResult<Components...> World::query() {
    SEED_ZONE("World::query");
    std::vector<Archetype*> matches;
    matches.reserve(m_archetypes.size());

    constexpr ComponentType required[] = { ComponentTraits<Components>::id... };
    constexpr size_t numRequired = sizeof...(Components);

    for (auto& [id, arch] : m_archetypes) {
        bool hasAll = true;
        for (size_t i = 0; i < numRequired; ++i) {
            if (!arch->hasComponent(required[i])) {
                hasAll = false;
                break;
            }
        }
        if (hasAll) {
            matches.push_back(arch.get());
        }
    }

    return QueryResult<Components...>(std::move(matches));
}

} // namespace seed::ecs
