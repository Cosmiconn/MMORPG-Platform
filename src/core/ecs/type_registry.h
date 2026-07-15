#pragma once

#include "core/ecs/component_array.h"
#include "core/ecs/component_traits.h"
#include "core/memory/allocator.h"
#include "core/profiling/seed_assert.h"
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace seed::ecs {

using ComponentArrayFactory = std::function<std::unique_ptr<IComponentArray>(seed::memory::Allocator*)>;

class TypeRegistry {
public:
    static TypeRegistry& instance();

    template<typename T>
    void registerComponent();

    std::unique_ptr<IComponentArray> createArray(ComponentType type, seed::memory::Allocator* alloc) const;
    bool isRegistered(ComponentType type) const;

private:
    TypeRegistry() = default;
    std::unordered_map<ComponentType, ComponentArrayFactory> m_factories;
    // BUGFIX (defense in depth): merkt sich, welcher C++-Typ zuletzt unter
    // einer ComponentType-id registriert wurde. Re-Registrierung DESSELBEN
    // Typs (z. B. am Anfang mehrerer Testfaelle) bleibt erlaubt; registriert
    // aber ein ANDERER Typ dieselbe id, ist das eine ID-Kollision, die sonst
    // still die Factory-Tabelle ueberschreibt und zu Type-Confusion in den
    // ComponentArray-Spalten fuehrt (z. B. ein unique_ptr-Feld wird als
    // POD-Struct reinterpretiert -> SIGSEGV). Das fangen wir hier laut ab.
    std::unordered_map<ComponentType, std::type_index> m_registeredTypes;
};

template<typename T>
void TypeRegistry::registerComponent() {
    const ComponentType id = ComponentTraits<T>::id;
    const std::type_index thisType = std::type_index(typeid(T));

    auto it = m_registeredTypes.find(id);
    if (it != m_registeredTypes.end() && it->second != thisType) {
        SEED_ASSERT(false,
            "TypeRegistry: ComponentType id collision - two different "
            "component types share the same id. Check SEED_REGISTER_COMPONENT_WITH_ID "
            "usages for a duplicate/overlapping id.");
        return;
    }
    m_registeredTypes.insert_or_assign(id, thisType);

    m_factories[id] = [](seed::memory::Allocator* a) {
        return std::make_unique<ComponentArray<T>>(a);
    };
}

} // namespace seed::ecs
