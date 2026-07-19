#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/ecs/world.h"
#include "core/ecs/archetype.h"
#include "core/ecs/archetype_manager.h"
#include "core/ecs/component_array.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/type_registry.h"
#include "core/profiling/tracy_seed.h"
#include "core/profiling/seed_assert.h"
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include "core/serialize/binary_reader.h"

namespace seed::serialize {

Snapshot Snapshot::capture(const seed::ecs::World& world) {
    SEED_ZONE("Snapshot::capture");

    BinaryWriter writer;
    SnapshotHeader header;
    header.entityCount = static_cast<uint32_t>(world.entityCount());

    uint32_t archetypeCount = 0;
    const auto& archMgr = world.archetypeManager();
    for (const auto& [id, arch] : archMgr) {
        if (arch->size() > 0) ++archetypeCount;
    }
    header.archetypeCount = archetypeCount;
    header.timestampUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    writer.writePOD(header);

    for (const auto& [id, arch] : archMgr) {
        if (arch->size() == 0) continue;

        writer.writeUInt32(id.hash);
        writer.writeUInt32(static_cast<uint32_t>(arch->componentTypes().size()));
        writer.writeUInt32(static_cast<uint32_t>(arch->size()));

        for (auto ctype : arch->componentTypes()) {
            writer.writeUInt32(ctype);
        }

        for (size_t i = 0; i < arch->size(); ++i) {
            seed::ecs::Entity e = arch->entityAt(i);
            writer.writeUInt32(e);

            for (auto ctype : arch->componentTypes()) {
                const void* compData = arch->getComponent(i, ctype);
                const auto* col = arch->getColumn(ctype);
                const auto& meta = col->meta();
                writer.writeBytes(compData, meta.size);
            }
        }
    }

    return Snapshot(writer.data());
}

std::vector<Snapshot::EntityState> Snapshot::parseEntities() const {
    SEED_ZONE("Snapshot::parseEntities");
    std::vector<EntityState> result;

    if (m_data.empty()) return result;

    BinaryReader reader(m_data);
    SnapshotHeader header = reader.readPOD<SnapshotHeader>();

    if (header.magic != SnapshotHeader::MAGIC) return result;

    result.reserve(header.entityCount);

    for (uint32_t a = 0; a < header.archetypeCount; ++a) {
        (void)reader.readUInt32(); // hash
        uint32_t compCount = reader.readUInt32();
        uint32_t entityCount = reader.readUInt32();

        std::vector<seed::ecs::ComponentType> types;
        types.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            types.push_back(reader.readUInt32());
        }

        for (uint32_t e = 0; e < entityCount; ++e) {
            EntityState state;
            state.entity = reader.readUInt32();
            state.types = types;
            state.componentData.reserve(compCount);

            for (uint32_t c = 0; c < compCount; ++c) {
                const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(types[c]);
                SEED_ASSERT(meta.size > 0, "Snapshot::parseEntities: unknown component type");
                std::vector<uint8_t> buffer(meta.size);
                reader.readBytes(buffer.data(), meta.size);
                state.componentData.push_back(std::move(buffer));
            }
            result.push_back(std::move(state));
        }
    }

    return result;
}

uint64_t Snapshot::timestampUs() const {
    if (m_data.size() < sizeof(SnapshotHeader)) return 0;
    BinaryReader reader(m_data);
    SnapshotHeader header = reader.readPOD<SnapshotHeader>();
    return header.timestampUs;
}

void Snapshot::apply(seed::ecs::World& world) const {
    SEED_ZONE("Snapshot::apply");
    SEED_ASSERT(!m_data.empty(), "Cannot apply empty snapshot");

    BinaryReader reader(m_data);
    SnapshotHeader header = reader.readPOD<SnapshotHeader>();

    SEED_ASSERT(header.magic == SnapshotHeader::MAGIC, "Invalid snapshot magic");
    SEED_ASSERT(header.version == SnapshotHeader::VERSION, "Unsupported snapshot version");

    for (uint32_t a = 0; a < header.archetypeCount; ++a) {
        uint32_t hash = reader.readUInt32();
        (void)hash;

        uint32_t compCount = reader.readUInt32();
        uint32_t entityCount = reader.readUInt32();

        std::vector<seed::ecs::ComponentType> types;
        types.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            types.push_back(reader.readUInt32());
        }
        std::sort(types.begin(), types.end());

        for (uint32_t e = 0; e < entityCount; ++e) {
            seed::ecs::Entity storedEntity = reader.readUInt32();
            (void)storedEntity;

            seed::ecs::Entity newEntity = world.createEntity();

            std::vector<std::vector<uint8_t>> componentData;
            componentData.reserve(compCount);

            for (uint32_t c = 0; c < compCount; ++c) {
                seed::ecs::ComponentType ctype = types[c];
                const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(ctype);
                SEED_ASSERT(meta.size > 0, "Snapshot::apply: unknown component type");
                std::vector<uint8_t> compBuffer(meta.size);
                reader.readBytes(compBuffer.data(), meta.size);
                componentData.push_back(std::move(compBuffer));
            }

            for (uint32_t c = 0; c < compCount; ++c) {
                world.addComponentRaw(newEntity, types[c], componentData[c].data());
            }
        }
    }
}

std::vector<uint8_t> Snapshot::serialize() const {
    return m_data;
}

Snapshot Snapshot::deserialize(const std::vector<uint8_t>& data) {
    return Snapshot(data);
}

Delta Snapshot::computeDelta(const Snapshot& older) const {
    SEED_ZONE("Snapshot::computeDelta");

    auto oldEntities = older.parseEntities();
    auto newEntities = this->parseEntities();

    std::unordered_map<seed::ecs::Entity, size_t> oldMap;
    for (size_t i = 0; i < oldEntities.size(); ++i) {
        oldMap[oldEntities[i].entity] = i;
    }

    std::unordered_map<seed::ecs::Entity, size_t> newMap;
    for (size_t i = 0; i < newEntities.size(); ++i) {
        newMap[newEntities[i].entity] = i;
    }

    BinaryWriter writer;
    Delta::Header header;
    header.baseSnapshotId = static_cast<uint32_t>(older.timestampUs() & 0xFFFFFFFF);
    header.newSnapshotId = static_cast<uint32_t>(this->timestampUs() & 0xFFFFFFFF);

    uint32_t changedEntities = 0;
    uint32_t newEntitiesCount = 0;
    uint32_t removedEntitiesCount = 0;

    for (const auto& newState : newEntities) {
        auto it = oldMap.find(newState.entity);
        if (it == oldMap.end()) {
            ++newEntitiesCount;
        } else {
            const auto& oldState = oldEntities[it->second];
            bool hasChanges = false;
            if (oldState.types.size() != newState.types.size()) {
                hasChanges = true;
            } else {
                for (size_t c = 0; c < newState.types.size(); ++c) {
                    size_t oldCompIdx = static_cast<size_t>(-1);
                    for (size_t oc = 0; oc < oldState.types.size(); ++oc) {
                        if (oldState.types[oc] == newState.types[c]) {
                            oldCompIdx = oc;
                            break;
                        }
                    }
                    if (oldCompIdx == static_cast<size_t>(-1)) {
                        hasChanges = true;
                        break;
                    }
                    if (oldState.componentData[oldCompIdx].size() != newState.componentData[c].size() ||
                        std::memcmp(oldState.componentData[oldCompIdx].data(),
                                   newState.componentData[c].data(),
                                   newState.componentData[c].size()) != 0) {
                        hasChanges = true;
                        break;
                    }
                }
            }
            if (hasChanges) ++changedEntities;
        }
    }

    for (const auto& oldState : oldEntities) {
        if (newMap.find(oldState.entity) == newMap.end()) {
            ++removedEntitiesCount;
        }
    }

    uint32_t totalDeltaEntities = changedEntities + newEntitiesCount + removedEntitiesCount;

    if (totalDeltaEntities > newEntities.size() / 2 && newEntities.size() > 0) {
        header.flags = 0x1;
        writer.writePOD(header);
        writer.writeUInt32(static_cast<uint32_t>(m_data.size()));
        writer.writeBytes(m_data.data(), m_data.size());
        return Delta(writer.data());
    }

    header.numChangedEntities = changedEntities;
    header.numNewEntities = newEntitiesCount;
    header.numRemovedEntities = removedEntitiesCount;
    writer.writePOD(header);

    for (const auto& newState : newEntities) {
        auto it = oldMap.find(newState.entity);
        if (it == oldMap.end()) continue;

        const auto& oldState = oldEntities[it->second];
        std::vector<uint32_t> changedComponents;
        std::vector<std::vector<uint8_t>> changedData;

        for (size_t c = 0; c < newState.types.size(); ++c) {
            size_t oldCompIdx = static_cast<size_t>(-1);
            for (size_t oc = 0; oc < oldState.types.size(); ++oc) {
                if (oldState.types[oc] == newState.types[c]) {
                    oldCompIdx = oc;
                    break;
                }
            }

            bool compChanged = true;
            if (oldCompIdx != static_cast<size_t>(-1)) {
                if (oldState.componentData[oldCompIdx].size() == newState.componentData[c].size() &&
                    std::memcmp(oldState.componentData[oldCompIdx].data(),
                               newState.componentData[c].data(),
                               newState.componentData[c].size()) == 0) {
                    compChanged = false;
                }
            }

            if (compChanged) {
                changedComponents.push_back(newState.types[c]);
                changedData.push_back(newState.componentData[c]);
            }
        }

        if (changedComponents.empty()) continue;

        writer.writeUInt32(newState.entity);
        writer.writeUInt32(static_cast<uint32_t>(changedComponents.size()));

        for (size_t i = 0; i < changedComponents.size(); ++i) {
            writer.writeUInt32(changedComponents[i]);
            writer.writeUInt32(static_cast<uint32_t>(changedData[i].size()));
            writer.writeBytes(changedData[i].data(), changedData[i].size());
        }
    }

    for (const auto& newState : newEntities) {
        if (oldMap.find(newState.entity) != oldMap.end()) continue;

        writer.writeUInt32(newState.entity);
        writer.writeUInt32(static_cast<uint32_t>(newState.types.size()));

        for (size_t c = 0; c < newState.types.size(); ++c) {
            writer.writeUInt32(newState.types[c]);
            writer.writeUInt32(static_cast<uint32_t>(newState.componentData[c].size()));
            writer.writeBytes(newState.componentData[c].data(), newState.componentData[c].size());
        }
    }

    for (const auto& oldState : oldEntities) {
        if (newMap.find(oldState.entity) != newMap.end()) continue;
        writer.writeUInt32(oldState.entity);
    }

    return Delta(writer.data());
}

} // namespace seed::serialize
