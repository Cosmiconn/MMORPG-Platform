#pragma once

#include "core/ecs/component_array.h"
#include "core/ecs/component_traits.h"
#include "core/memory/allocator.h"
#include <functional>
#include <memory>
#include <unordered_map>

namespace seed::ecs {

using ComponentArrayFactory = std::function<std::unique_ptr<IComponentArray>(seed::memory::Allocator*)>;

class TypeRegistry {
public:
    TypeRegistry() = default;
    ~TypeRegistry() = default;

    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    template<typename T>
    void registerComponent();

    std::unique_ptr<IComponentArray> createArray(ComponentType type, seed::memory::Allocator* alloc) const;
    bool isRegistered(ComponentType type) const;

    // Reset for test isolation
    void clear() { m_factories.clear(); }

private:
    std::unordered_map<ComponentType, ComponentArrayFactory> m_factories;
};

template<typename T>
void TypeRegistry::registerComponent() {
    m_factories[ComponentTraits<T>::id] = [](seed::memory::Allocator* a) {
        return std::make_unique<ComponentArray<T>>(a);
    };
}

} // namespace seed::ecs
