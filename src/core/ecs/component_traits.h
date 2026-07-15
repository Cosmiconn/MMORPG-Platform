#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace seed::ecs {

using ComponentType = uint32_t;

// ---------------------------------------------------------------------------
// ComponentMeta – type-erased vtable for component operations
// ---------------------------------------------------------------------------
struct ComponentMeta {
    size_t size = 0;
    size_t alignment = 1;
    const char* name = "unknown";

    void (*construct)(void* ptr) = nullptr;
    void (*destruct)(void* ptr) = nullptr;
    void (*move)(void* dst, void* src) = nullptr;
    void (*copy)(void* dst, const void* src) = nullptr;

    bool isTriviallyRelocatable = false;
    bool isMoveOnly = false;
};

// ---------------------------------------------------------------------------
// ComponentTraits<T> – template specialization per component type
// ---------------------------------------------------------------------------
template<typename T>
struct ComponentTraits {
    static constexpr ComponentType id = 0; // Must be specialized
};

// ---------------------------------------------------------------------------
// getComponentMeta<T>() – builds a ComponentMeta for any T
// ---------------------------------------------------------------------------
template<typename T>
ComponentMeta getComponentMeta() {
    ComponentMeta meta{};
    meta.size = sizeof(T);
    meta.alignment = alignof(T);
    meta.name = typeid(T).name();

    meta.construct = [](void* ptr) { new (ptr) T(); };
    meta.destruct = [](void* ptr) { static_cast<T*>(ptr)->~T(); };

    // Move: placement-new move-construct at dst, then destruct src
    meta.move = [](void* dst, void* src) {
        T* srcPtr = static_cast<T*>(src);
        new (dst) T(std::move(*srcPtr));
        srcPtr->~T();
    };

    // Copy: placement-new copy-construct at dst
    meta.copy = [](void* dst, const void* src) {
        new (dst) T(*static_cast<const T*>(src));
    };

    meta.isTriviallyRelocatable = std::is_trivially_copyable_v<T>;
    meta.isMoveOnly = !std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>;

    return meta;
}

// Specialization for trivially copyable types: use memcpy for move/copy
// (handled at runtime via isTriviallyRelocatable flag)

} // namespace seed::ecs
