#pragma once

#include "core/ecs/component_traits.h"
#include <unordered_map>

namespace seed::ecs {

class TypeRegistry {
public:
    template<typename T>
    void registerComponent() {
        ComponentType id = ComponentTraits<T>::id;
        if (id == 0) return; // Not specialized
        m_metas[id] = getComponentMeta<T>();
    }

    const ComponentMeta* getMeta(ComponentType id) const {
        auto it = m_metas.find(id);
        return (it != m_metas.end()) ? &it->second : nullptr;
    }

private:
    std::unordered_map<ComponentType, ComponentMeta> m_metas;
};

} // namespace seed::ecs
