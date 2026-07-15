#pragma once

#include <cstdint>

namespace seed::ecs {

using Entity = uint32_t;
constexpr Entity INVALID_ENTITY = 0xFFFFFFFF;

inline uint32_t entityIndex(Entity e) { return e & 0x00FFFFFF; }
inline uint8_t entityVersion(Entity e) { return (e >> 24) & 0xFF; }
inline Entity makeEntity(uint32_t index, uint8_t version) {
    return (static_cast<Entity>(version) << 24) | (index & 0x00FFFFFF);
}

} // namespace seed::ecs
