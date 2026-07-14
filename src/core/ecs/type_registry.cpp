#include "core/profiling/seed_assert.h"
#include "core/ecs/type_registry.h"

namespace seed::ecs {

std::unique_ptr<IComponentArray> TypeRegistry::createArray(ComponentType type,
                                                            seed::memory::Allocator* alloc) const {
    auto it = m_factories.find(type);
    if (it != m_factories.end()) {
        return it->second(alloc);
    }
    return nullptr;
}

bool TypeRegistry::isRegistered(ComponentType type) const {
    return m_factories.find(type) != m_factories.end();
}

} // namespace seed::ecs
