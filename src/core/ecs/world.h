#pragma once

// ---------------------------------------------------------------------------
// Changelog
// ---------------------------------------------------------------------------
// 2026-07-14  Refactor: Scoped TypeRegistry (Option C)
//   TypeRegistry ist kein Singleton mehr, sondern ein Member von World.
//   Das eliminiert Test-Interferenz zwischen ECS-Tests.
//   ArchetypeManager erhält einen Pointer auf die World-eigene Registry.
//
// 2026-07-14  Bugfix: addComponent Double-Destruct entfernt
//   Der 2026-07-13 Fix war doppelt destruktiv: nach moveEntity enthält
//   der Slot bereits das bewegte Objekt, kein default-konstruiertes.
//   Ein zusätzliches newSlot->~T() zerstört das gerade bewegte Objekt
//   → Double-Free bei move-only Typen (unique_ptr).
//   Korrektur: placement-new überschreibt direkt, kein Destruct davor.
// ---------------------------------------------------------------------------

#include "core/profiling/seed_assert.h"
#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
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
#include "core/diagnostics/ecs_validator.h"
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace seed::ecs {

using seed::diagnostics::EventType;

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

    // Scoped TypeRegistry - register components per World instance
    TypeRegistry& typeRegistry() noexcept { return m_typeRegistry; }
    const TypeRegistry& typeRegistry() const noexcept { return m_typeRegistry; }

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

    void dump() const;
    bool validateInvariants() const;

    void* getComponentRaw(Entity e, ComponentType type);
    void setComponentRaw(Entity e, ComponentType type, const void* data);

private:
    struct EntitySlot {
        Entity entity;
        bool alive;
        uint32_t nextFree;
    };

    seed::memory::Allocator* m_allocator;
    TypeRegistry m_typeRegistry;  // Scoped per World instance
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
    void moveEntity(Entity e, Archetype* oldArch, size_t oldIndex, Archetype* newArch);
};

template<typename T, typename... Args>
T* World::addComponent(Entity e, Args&&... args) {
    SEED_ZONE("World::addComponent");
    SEED_DIAG_EVENT(EventType::ComponentAdd, e, 0, ComponentTraits<T>::id, 0,
                    "addComponent<T> start", __FILE__, __LINE__);

    if (!isAlive(e)) {
        SEED_DIAG_EVENT(EventType::InvariantFail, e, 0, ComponentTraits<T>::id, 0,
                        "addComponent on dead entity", __FILE__, __LINE__);
        return nullptr;
    }

    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* oldArch = getArchetype(rec.archetypeId);
    ComponentType newType = ComponentTraits<T>::id;

    // Entity has no components yet (empty archetype, hash=0)
    if (oldArch == nullptr) {
        std::vector<ComponentType> newTypes = {newType};
        Archetype* newArch = findOrCreateArchetype(newTypes);
        SEED_DIAG_EVENT(EventType::ArchetypeCreate, e, newArch->id().hash, newType, 0,
                        "new archetype for first component", __FILE__, __LINE__);
        size_t newIndex = newArch->addEntity(e);
        m_records[entityIndex(e)] = {newArch->id(), static_cast<uint32_t>(newIndex)};
        T* newSlot = newArch->getComponent<T>(newIndex);
        // Destroy default-constructed component before placement-new
        newArch->destructComponentAt(newIndex, ComponentTraits<T>::id);
        new (newSlot) T(std::forward<Args>(args)...);
        SEED_DIAG_EVENT(EventType::ComponentAdd, e, newArch->id().hash, newType, static_cast<uint32_t>(newIndex),
                        "addComponent<T> first component done", __FILE__, __LINE__);
        return newSlot;
    }

    // Check if component already exists on this entity -> update value
    if (oldArch->hasComponent(newType)) {
        T* slot = oldArch->getComponent<T>(rec.index);
        *slot = T(std::forward<Args>(args)...);
        SEED_DIAG_EVENT(EventType::ComponentAdd, e, oldArch->id().hash, newType, rec.index,
                        "addComponent<T> update existing", __FILE__, __LINE__);
        return slot;
    }

    std::vector<ComponentType> newTypes = oldArch->componentTypes();
    newTypes.push_back(newType);
    std::sort(newTypes.begin(), newTypes.end());

    Archetype* newArch = findOrCreateArchetype(newTypes);
    SEED_DIAG_EVENT(EventType::ArchetypeCreate, e, newArch->id().hash, newType, 0,
                    "archetype move for new component", __FILE__, __LINE__);
    moveEntity(e, oldArch, rec.index, newArch);

    // After moveEntity, the slot contains the MOVED object from old archetype.
    // The moveComponent in moveEntity already did: destruct(dst) + move(dst, src).
    // So the slot now has a valid, moved-constructed T.
    // We can directly placement-new over it — the old object's lifetime ends
    // and the new object begins at the same address.
    T* newSlot = newArch->getComponent<T>(m_records[entityIndex(e)].index);
    new (newSlot) T(std::forward<Args>(args)...);
    SEED_DIAG_EVENT(EventType::ComponentAdd, e, newArch->id().hash, newType, m_records[entityIndex(e)].index,
                    "addComponent<T> archetype move done", __FILE__, __LINE__);
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
    SEED_DIAG_EVENT(seed::diagnostics::EventType::ComponentRemove, e, 0,
                    ComponentTraits<T>::id, 0,
                    "removeComponent<T> start", __FILE__, __LINE__);
    if (!isAlive(e)) {
        SEED_DIAG_EVENT(seed::diagnostics::EventType::InvariantFail, e, 0,
                        ComponentTraits<T>::id, 0,
                        "removeComponent on dead entity", __FILE__, __LINE__);
        return;
    }

    const EntityRecord& rec = m_records[entityIndex(e)];
    Archetype* oldArch = getArchetype(rec.archetypeId);
    if (oldArch == nullptr) return;

    ComponentType remType = ComponentTraits<T>::id;
    std::vector<ComponentType> newTypes;
    newTypes.reserve(oldArch->componentTypes().size() - 1);

    for (ComponentType ct : oldArch->componentTypes()) {
        if (ct != remType) {
            newTypes.push_back(ct);
        }
    }

    if (newTypes.size() == oldArch->componentTypes().size()) return;

    if (newTypes.empty()) {
        oldArch->removeEntityByIndex(rec.index);
        if (rec.index < oldArch->size()) {
            Entity moved = oldArch->entityAt(rec.index);
            m_records[entityIndex(moved)].index = static_cast<uint32_t>(rec.index);
        }
        m_records[entityIndex(e)] = {ArchetypeId{0, {}}, 0};
        return;
    }

    Archetype* newArch = findOrCreateArchetype(newTypes);
    moveEntity(e, oldArch, rec.index, newArch);
}

template<typename... Components>
QueryResult<Components...> World::query() {
    SEED_ZONE("World::query");
    std::vector<Archetype*> matches;
    matches.reserve(m_archetypeManager->archetypeCount());

    constexpr ComponentType required[] = { ComponentTraits<Components>::id... };
    constexpr size_t numRequired = sizeof...(Components);

    for (const auto& [id, arch] : *m_archetypeManager) {
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
