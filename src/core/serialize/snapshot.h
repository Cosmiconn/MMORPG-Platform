#pragma once

#include "core/serialize/binary_writer.h"
#include "core/serialize/binary_reader.h"
#include "core/ecs/entity.h"
#include <vector>
#include <cstdint>

namespace seed::ecs {
    class World;
}

namespace seed::serialize {

class Delta;

// ---------------------------------------------------------------------------
// SnapshotHeader
// ---------------------------------------------------------------------------
struct SnapshotHeader {
    static constexpr uint32_t MAGIC = 0x53454544; // "SEED"
    static constexpr uint32_t VERSION = 1;

    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t entityCount = 0;
    uint32_t archetypeCount = 0;
    uint64_t timestampUs = 0; // microsecond timestamp for delta baselines
    uint32_t schemaVersion = 1; // For forward compatibility
};

// ---------------------------------------------------------------------------
// Snapshot
// ---------------------------------------------------------------------------
class Snapshot {
public:
    Snapshot() = default;
    explicit Snapshot(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    // Capture full world state
    static Snapshot capture(const seed::ecs::World& world);

    // Apply snapshot to world (creates new entities, does NOT clear world first)
    void apply(seed::ecs::World& world) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static Snapshot deserialize(const std::vector<uint8_t>& data);

    // Accessors
    const std::vector<uint8_t>& data() const { return m_data; }
    size_t size() const { return m_data.size(); }
    uint64_t timestampUs() const;

    // Entity-level delta computation
    Delta computeDelta(const Snapshot& older) const;

    // Internal: parse snapshot into entity map for delta computation
    struct EntityState {
        seed::ecs::Entity entity;
        std::vector<seed::ecs::ComponentType> types;
        std::vector<std::vector<uint8_t>> componentData;
    };

    std::vector<EntityState> parseEntities() const;

private:
    std::vector<uint8_t> m_data;
};

// ---------------------------------------------------------------------------
// Delta – entity-level compressed delta
// ---------------------------------------------------------------------------
class Delta {
public:
    Delta() = default;
    explicit Delta(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    // Apply this delta to a world
    void apply(seed::ecs::World& world) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static Delta deserialize(const std::vector<uint8_t>& data);

    size_t size() const { return m_data.size(); }

    // Header access
    struct Header {
        uint32_t magic = 0x44454C54; // "DELT"
        uint32_t version = 1;
        uint32_t baseSnapshotId = 0;
        uint32_t newSnapshotId = 0;
        uint32_t numChangedEntities = 0;
        uint32_t numNewEntities = 0;
        uint32_t numRemovedEntities = 0;
        uint32_t flags = 0; // 0x1 = full fallback
    };

    Header readHeader() const;

private:
    std::vector<uint8_t> m_data;
};

} // namespace seed::serialize
