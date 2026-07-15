#include "core/ecs/archetype_manager.h"
#include "core/profiling/seed_assert.h"

namespace seed::ecs {

ArchetypeManager::ArchetypeManager(seed::memory::Allocator* allocator, TypeRegistry* typeRegistry)
    : m_allocator(allocator), m_typeRegistry(typeRegistry) {
}

ArchetypeManager::~ArchetypeManager() = default;

Archetype* ArchetypeManager::getArchetype(ArchetypeId id) {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? it->second.get() : nullptr;
}

const Archetype* ArchetypeManager::getArchetype(ArchetypeId id) const {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? it->second.get() : nullptr;
}

Archetype* ArchetypeManager::findOrCreateArchetype(const std::vector<ComponentType>& types) {
    ArchetypeId id = makeArchetypeId(types);
    auto it = m_archetypes.find(id);
    if (it != m_archetypes.end()) {
        return it->second.get();
    }

    std::vector<std::unique_ptr<IComponentArray>> columns;
    for (ComponentType ct : types) {
        const ComponentMeta* meta = m_typeRegistry->getMeta(ct);
        SEED_ASSERT(meta != nullptr, "Component type not registered");

        // Factory dispatch based on type ID
        // In real code, this would use a type registry factory
        // For now, we rely on the template instantiation in world.h
        (void)meta; // suppress unused warning in this simplified version
    }

    auto arch = std::make_unique<Archetype>(id, types, std::move(columns), m_allocator);
    Archetype* ptr = arch.get();
    m_archetypes.emplace(id, std::move(arch));
    return ptr;
}

} // namespace seed::ecs
