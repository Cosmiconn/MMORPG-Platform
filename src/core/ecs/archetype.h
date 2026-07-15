#pragma once

#include "core/profiling/seed_assert.h"
#include "core/ecs/component_array.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/entity.h"
#include "core/memory/allocator.h"
#include "core/profiling/tracy_seed.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace seed::ecs {

struct ArchetypeId {
    uint32_t hash = 0;
    std::vector<ComponentType> signature;

    bool operator==(const ArchetypeId& other) const noexcept {
        return hash == other.hash && signature == other.signature;
    }
    bool operator!=(const ArchetypeId& other) const noexcept {
        return !(*this == other);
    }
};

struct ArchetypeIdHash {
    std::size_t operator()(const ArchetypeId& id) const noexcept {
        return std::hash<uint32_t>{}(id.hash);
    }
};

inline ArchetypeId makeArchetypeId(const std::vector<ComponentType>& types) {
    uint32_t h = 2166136261u;
    for (ComponentType t : types) {
        h ^= t;
        h *= 16777619u;
    }
    return ArchetypeId{h, types};
}

class Archetype {
public:
    Archetype(ArchetypeId id,
              std::vector<ComponentType> componentTypes,
              std::vector<std::unique_ptr<IComponentArray>> columns,
              seed::memory::Allocator* allocator);
    ~Archetype();

    ArchetypeId id() const noexcept { return m_id; }
    size_t size() const noexcept { return m_entityCount; }
    const std::vector<ComponentType>& componentTypes() const noexcept { return m_componentTypes; }
    const std::vector<std::unique_ptr<IComponentArray>>& getColumns() const { return m_columns; }

    bool hasComponent(ComponentType type) const noexcept;
    size_t addEntity(Entity e);
    void removeEntityByIndex(size_t index);
    Entity entityAt(size_t index) const;
    size_t findEntityIndex(Entity e) const;

    void* getComponent(size_t index, ComponentType type);
    const void* getComponent(size_t index, ComponentType type) const;
    void setComponent(size_t index, ComponentType type, const void* data);

    IComponentArray* getColumn(ComponentType type);
    const IComponentArray* getColumn(ComponentType type) const;

    void moveComponent(size_t dstIndex, ComponentType type, IComponentArray* src, size_t srcIndex);
    void destructComponentAt(size_t index, ComponentType type);

    size_t capacity() const;
    void reserve(size_t n);

private:
    ArchetypeId m_id;
    std::vector<ComponentType> m_componentTypes;
    std::vector<std::unique_ptr<IComponentArray>> m_columns;
    std::vector<Entity> m_entities;
    size_t m_entityCount = 0;
    seed::memory::Allocator* m_allocator;
};

} // namespace seed::ecs
