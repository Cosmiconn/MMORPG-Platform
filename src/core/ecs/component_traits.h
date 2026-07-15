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
            // src bleibt im moved-from Zustand; Aufrufer räumt auf
        },
        .copy = [](void* dst, const void* src) {
            (void)dst; (void)src; // silence MSVC C4100 for non-copyable types
            if constexpr (std::is_copy_constructible_v<T>) {
                new (dst) T(*static_cast<const T*>(src));
            }
            // For non-copyable types, copy is a no-op (should never be called)
        },
    };
}

// ---------------------------------------------------------------------------
// Macros for easy component registration
// ---------------------------------------------------------------------------

// Use this when you want to assign an explicit ID (e.g. for networking)
// Usage: SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
#define SEED_REGISTER_COMPONENT_WITH_ID(T, ID) \
    template<> struct seed::ecs::ComponentTraits<T> { \
        static constexpr seed::ecs::ComponentType id = (ID); \
        static constexpr std::string_view name = #T; \
    };

// ---------------------------------------------------------------------------
// BUGFIX (root cause of CI-Crash ECS_Component_MoveOnlyType / SIGSEGV):
// __COUNTER__ zaehlt pro Translation Unit ab der erstbenutzten Stelle hoch
// und haengt davon ab, wie oft es zuvor bereits in dieser Datei/den
// eingebundenen Headern (z. B. <doctest/doctest.h>) verwendet wurde. Der
// resultierende Wert ist daher NICHT garantiert unterschiedlich von
// manuell vergebenen IDs (SEED_REGISTER_COMPONENT_WITH_ID(Position, 1) etc.),
// die im selben Uebersetzungseinheit-weiten TypeRegistry-Namensraum leben.
// Kollidieren zwei Typen auf derselben ComponentType-id, ueberschreibt die
// zuletzt registrierte Factory in TypeRegistry die vorige: Spalten werden
// dann mit der FALSCHEN ComponentMeta (falsche Groesse/Move-Funktion)
// erzeugt, wodurch z. B. ein std::unique_ptr-Feld als drei Floats (oder
// umgekehrt) reinterpretiert wird -> nicht-nullwertiger, aber wilder
// Pointer -> SIGSEGV beim Dereferenzieren (reproduziert und verifiziert).
//
// Fix: Auto-IDs werden in einen reservierten oberen Bereich verschoben, der
// von "normalen", klein vergebenen expliziten IDs (Networking etc.) nicht
// erreicht wird. Explizite IDs sollten daher immer < SEED_AUTO_ID_BASE
// bleiben.
// ---------------------------------------------------------------------------
inline constexpr seed::ecs::ComponentType SEED_AUTO_ID_BASE = 0x40000000u;

// Use this for auto-generated IDs (local-only components)
// Usage: SEED_REGISTER_COMPONENT(UniqueResource)
#define SEED_REGISTER_COMPONENT(T) \
    SEED_REGISTER_COMPONENT_WITH_ID(T, (seed::ecs::SEED_AUTO_ID_BASE + static_cast<seed::ecs::ComponentType>(__COUNTER__)))

} // namespace seed::ecs
