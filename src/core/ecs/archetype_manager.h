#pragma once

#include "core/ecs/archetype.h"
#include "core/ecs/type_registry.h"
#include "core/memory/allocator.h"
#include <memory>
#include <unordered_map>

namespace seed::ecs {

class ArchetypeManager {
public:
    ArchetypeManager(seed::memory::Allocator* allocator, TypeRegistry* typeRegistry);
    ~ArchetypeManager();

    Archetype* getArchetype(ArchetypeId id);
    const Archetype* getArchetype(ArchetypeId id) const;
    Archetype* findOrCreateArchetype(const std::vector<ComponentType>& types);

    size_t archetypeCount() const { return m_archetypes.size(); }

    auto begin() { return m_archetypes.begin(); }
    auto end() { return m_archetypes.end(); }
    auto begin() const { return m_archetypes.begin(); }
    auto end() const { return m_archetypes.end(); }

private:
    seed::memory::Allocator* m_allocator;
    TypeRegistry* m_typeRegistry;
    std::unordered_map<ArchetypeId, std::unique_ptr<Archetype>, ArchetypeIdHash> m_archetypes;
};

} // namespace seed::ecs
