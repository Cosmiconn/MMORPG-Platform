#pragma once

#include "core/serialize/binary_writer.h"
#include "core/ecs/entity.h"
#include <vector>
#include <cstdint>

namespace seed::ecs {
    class World;
}

namespace seed::serialize {

class Delta;

struct SnapshotHeader {
    static constexpr uint32_t MAGIC = 0x53454544; // "SEED"
    static constexpr uint32_t VERSION = 1;

    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t entityCount = 0;
    uint32_t archetypeCount = 0;
    uint64_t timestampUs = 0;
    uint32_t schemaVersion = 1;
};

class Snapshot {
public:
    Snapshot() = default;
    explicit Snapshot(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    static Snapshot capture(const seed::ecs::World& world);
    void apply(seed::ecs::World& world) const;

    std::vector<uint8_t> serialize() const;
    static Snapshot deserialize(const std::vector<uint8_t>& data);

    const std::vector<uint8_t>& data() const { return m_data; }
    size_t size() const { return m_data.size(); }
    uint64_t timestampUs() const;

    Delta computeDelta(const Snapshot& older) const;

    struct EntityState {
        seed::ecs::Entity entity;
        std::vector<seed::ecs::ComponentType> types;
        std::vector<std::vector<uint8_t>> componentData;
    };

    std::vector<EntityState> parseEntities() const;

private:
    std::vector<uint8_t> m_data;
};

class Delta {
public:
    Delta() = default;
    explicit Delta(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    void apply(seed::ecs::World& world) const;

    std::vector<uint8_t> serialize() const;
    static Delta deserialize(const std::vector<uint8_t>& data);

    size_t size() const { return m_data.size(); }

    struct Header {
        uint32_t magic = 0x44454C54; // "DELT"
        uint32_t version = 1;
        uint32_t baseSnapshotId = 0;
        uint32_t newSnapshotId = 0;
        uint32_t numChangedEntities = 0;
        uint32_t numNewEntities = 0;
        uint32_t numRemovedEntities = 0;
        uint32_t flags = 0;
    };

    Header readHeader() const;

private:
    std::vector<uint8_t> m_data;
};

} // namespace seed::serialize
