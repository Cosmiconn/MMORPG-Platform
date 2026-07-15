#pragma once

#include "core/ecs/archetype.h"
#include "core/ecs/archetype_manager.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/entity.h"
#include "core/ecs/query.h"
#include "core/ecs/system.h"
#include "core/ecs/type_registry.h"
#include "core/memory/allocator.h"
#include "core/profiling/tracy_seed.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

namespace seed::ecs {

struct EntitySlot {
    Entity entity = INVALID_ENTITY;
    bool alive = false;
    uint32_t nextFree = INVALID_ENTITY;
};

struct EntityRecord {
    ArchetypeId archetypeId;
    uint32_t index;
};

class World {
public:
    explicit World(seed::memory::Allocator* allocator);
    ~World();

    // Entity management
    Entity createEntity();
    void destroyEntity(Entity e);
    bool isAlive(Entity e) const;
    size_t entityCount() const noexcept { return m_aliveCount; }

    // Components
    template<typename T, typename... Args>
    T* addComponent(Entity e, Args&&... args) {
        SEED_ZONE("World::addComponent");
        if (!isAlive(e)) return nullptr;

        ComponentType ct = ComponentTraits<T>::id;
        if (ct == 0) return nullptr;

        const EntityRecord& rec = m_records[entityIndex(e)];
        Archetype* oldArch = (rec.archetypeId.hash != 0) ? getArchetype(rec.archetypeId) : nullptr;

        if (oldArch && oldArch->hasComponent(ct)) {
            return static_cast<T*>(getComponentRaw(e, ct));
        }

        std::vector<ComponentType> newTypes;
        if (oldArch) {
            newTypes = oldArch->componentTypes();
        }
        newTypes.push_back(ct);
        std::sort(newTypes.begin(), newTypes.end());

        Archetype* newArch = m_archetypeManager->findOrCreateArchetype(newTypes);

        if (oldArch) {
            moveEntity(e, oldArch, rec.index, newArch);
        } else {
            size_t idx = newArch->addEntity(e);
            m_records[entityIndex(e)] = {newArch->id(), static_cast<uint32_t>(idx)};
        }

        void* ptr = getComponentRaw(e, ct);
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return static_cast<T*>(ptr);
    }

    template<typename T>
    T* getComponent(Entity e) {
        if (!isAlive(e)) return nullptr;
        return static_cast<T*>(getComponentRaw(e, ComponentTraits<T>::id));
    }

    template<typename T>
    const T* getComponent(Entity e) const {
        if (!isAlive(e)) return nullptr;
        return static_cast<const T*>(getComponentRaw(e, ComponentTraits<T>::id));
    }

    template<typename T>
    void removeComponent(Entity e) {
        SEED_ZONE("World::removeComponent");
        if (!isAlive(e)) return;

        ComponentType ct = ComponentTraits<T>::id;
        const EntityRecord& rec = m_records[entityIndex(e)];
        Archetype* oldArch = getArchetype(rec.archetypeId);
        if (!oldArch || !oldArch->hasComponent(ct)) return;

        std::vector<ComponentType> newTypes = oldArch->componentTypes();
        newTypes.erase(std::remove(newTypes.begin(), newTypes.end(), ct), newTypes.end());

        Archetype* newArch = m_archetypeManager->findOrCreateArchetype(newTypes);
        moveEntity(e, oldArch, rec.index, newArch);
    }

    // Systems
    void registerSystem(std::unique_ptr<System> system);
    void update(float deltaTime);

    // Queries
    template<typename... Components>
    QueryResult<Components...> query() {
        return QueryResult<Components...>(this, m_archetypeManager.get());
    }

    // Type registry
    TypeRegistry& typeRegistry() noexcept { return m_typeRegistry; }
    const TypeRegistry& typeRegistry() const noexcept { return m_typeRegistry; }

    // Archetype access (for diagnostics)
    Archetype* getArchetype(ArchetypeId id);
    const Archetype* getArchetype(ArchetypeId id) const;
    const ArchetypeManager& getArchetypeManager() const { return *m_archetypeManager; }
    ArchetypeManager& getArchetypeManager() { return *m_archetypeManager; }

    // Diagnostics access
    const std::vector<EntitySlot>& getEntities() const { return m_entities; }
    const std::vector<EntityRecord>& getRecords() const { return m_records; }
    size_t aliveCount() const { return m_aliveCount; }

    // Validation
    bool validateInvariants() const;
    void dump() const;
    std::string dumpToString() const;

private:
    void moveEntity(Entity e, Archetype* oldArch, size_t oldIndex, Archetype* newArch);
    void* getComponentRaw(Entity e, ComponentType type);
    void setComponentRaw(Entity e, ComponentType type, const void* data);

    seed::memory::Allocator* m_allocator;
    std::vector<EntitySlot> m_entities;
    std::vector<EntityRecord> m_records;
    uint32_t m_nextFree;
    size_t m_aliveCount;
    uint8_t m_nextVersion;

    TypeRegistry m_typeRegistry;
    std::unique_ptr<ArchetypeManager> m_archetypeManager;
    std::vector<std::unique_ptr<System>> m_systems;
};

} // namespace seed::ecs
