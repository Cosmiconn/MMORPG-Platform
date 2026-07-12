#pragma once

#include "core/ecs/entity.h"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace seed::ecs {

// ---------------------------------------------------------------------------
// ComponentType
// ---------------------------------------------------------------------------
using ComponentType = uint32_t;

// ---------------------------------------------------------------------------
// ComponentMeta
// ---------------------------------------------------------------------------
struct ComponentMeta {
    ComponentType id;
    size_t size;
    size_t alignment;
    std::string_view name;

    void (*construct)(void* ptr);
    void (*destruct)(void* ptr);
    void (*move)(void* dst, void* src);
    void (*copy)(void* dst, const void* src);
};

// ---------------------------------------------------------------------------
// ComponentTraits – specialize per component type
// ---------------------------------------------------------------------------
template<typename T>
struct ComponentTraits;

// Helper to register a component
template<typename T>
inline constexpr ComponentMeta getComponentMeta() {
    using Traits = ComponentTraits<T>;
    return ComponentMeta{
        .id = Traits::id,
        .size = sizeof(T),
        .alignment = alignof(T),
        .name = Traits::name,
        .construct = [](void* p) { new (p) T(); },
        .destruct = [](void* p) { static_cast<T*>(p)->~T(); },
        .move = [](void* dst, void* src) {
            new (dst) T(std::move(*static_cast<T*>(src)));
            static_cast<T*>(src)->~T();
        },
        .copy = [](void* dst, const void* src) {
            new (dst) T(*static_cast<const T*>(src));
        },
    };
}

// ---------------------------------------------------------------------------
// Macros for easy component registration
// ---------------------------------------------------------------------------

// Use this when you want to assign an explicit ID (e.g. for networking)
#define SEED_REGISTER_COMPONENT_WITH_ID(T, ID)     template<> struct seed::ecs::ComponentTraits<T> {         static constexpr seed::ecs::ComponentType id = (ID);         static constexpr std::string_view name = #T;     }

// Use this for auto-generated IDs (local-only components)
#define SEED_REGISTER_COMPONENT(T)     SEED_REGISTER_COMPONENT_WITH_ID(T, __COUNTER__)

} // namespace seed::ecs
