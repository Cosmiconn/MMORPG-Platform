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

    // GAP-FIX (Cross-Platform-Determinismus): writePOD auf SnapshotHeader
    // schreibt auch Struct-Padding (4 Bytes nach schemaVersion), die
    // uninitialisierten Stack-Werte enthalten. Feld-fuer-Feld schreibt
    // exakt 28 Bytes, padding-frei, plattformidentisch.
    writer.writeUInt32(header.magic);
    writer.writeUInt32(header.version);
    writer.writeUInt32(header.entityCount);
    writer.writeUInt32(header.archetypeCount);
    writer.writeUInt64(header.timestampUs);
    writer.writeUInt32(header.schemaVersion);

    // GAP-FIX (Cross-Platform-Byte-Vergleich): std::unordered_map
    // Iterationsreihenfolge ist nicht spezifiziert und variiert zwischen
    // libstdc++ (Linux/GCC) und MSVC-STL (Windows). Damit Linux- und Windows-
    // Snapshots byte-identisch sind, muessen Archetypes deterministisch
    // sortiert geschrieben werden.
    std::vector<const seed::ecs::Archetype*> archetypes;
    archetypes.reserve(archetypeCount);
    for (const auto& [id, arch] : archMgr) {
        if (arch->size() > 0) archetypes.push_back(arch.get());
    }
    std::sort(archetypes.begin(), archetypes.end(),
        [](const seed::ecs::Archetype* a, const seed::ecs::Archetype* b) {
            return a->id().hash < b->id().hash;
        });

    for (const seed::ecs::Archetype* arch : archetypes) {
        if (arch->size() == 0) continue;

        const auto types = arch->componentTypes();  // cache once
        // NOTE: Archetype-Hash wurde aus dem Wire-Format entfernt
        // (war toter Code - wurde nur geschrieben und sofort verworfen).
        // Das eliminiert eine plattformabhaengige Differenzquelle und
        // macht das Format fuer Phase 1 (Netzwerk) deterministisch.
        writer.writeUInt32(static_cast<uint32_t>(types.size()));
        writer.writeUInt32(static_cast<uint32_t>(arch->size()));

        for (auto ctype : types) {
            writer.writeUInt32(ctype);
        }

        // GAP-FIX (2026-07-22, P1-3 Schema-Migration): gespeicherte
        // Komponenten-Groesse pro Typ mitschreiben (einmal pro Archetype).
        // Ohne das muesste Snapshot::apply()/parseEntities() beim Lesen
        // blind die AKTUELL registrierte Groesse annehmen - das bricht,
        // sobald sich ein Schema additiv erweitert (aeltere Snapshots haben
        // dann weniger Bytes pro Komponente als die neue Struct-Groesse).
        for (auto ctype : types) {
            const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(ctype);
            writer.writeUInt32(static_cast<uint32_t>(meta.size));
        }

        // Cache columns and meta before entity loop to avoid repeated lookups
        struct CachedColumn {
            const seed::ecs::IComponentArray* col;
            size_t size;
        };
        std::vector<CachedColumn> cachedCols;
        cachedCols.reserve(types.size());
        for (auto ctype : types) {
            const auto* col = arch->getColumn(ctype);
            cachedCols.push_back({col, col->meta().size});
        }

        for (size_t i = 0; i < arch->size(); ++i) {
            seed::ecs::Entity e = arch->entityAt(i);
            writer.writeUInt32(e);

            for (size_t c = 0; c < types.size(); ++c) {
                const void* compData = arch->getComponent(i, types[c]);
                writer.writeBytes(compData, cachedCols[c].size);
            }
        }
    }

    Snapshot snap(writer.data());
    snap.entityCount = header.entityCount;
    snap.entityStates = snap.parseEntities();

    for (const auto& [id, arch] : archMgr) {
        if (arch->size() == 0) continue;
        for (auto ctype : arch->componentTypes()) {
            if (std::find(snap.componentTypes.begin(), snap.componentTypes.end(), ctype) == snap.componentTypes.end()) {
                snap.componentTypes.push_back(ctype);
            }
        }
    }

    return snap;
}

std::vector<Snapshot::EntityState> Snapshot::parseEntities() const {
    SEED_ZONE("Snapshot::parseEntities");
    std::vector<EntityState> result;

    if (m_data.empty()) return result;

    BinaryReader reader(m_data);
    SnapshotHeader header;
    header.magic = reader.readUInt32();
    header.version = reader.readUInt32();
    header.entityCount = reader.readUInt32();
    header.archetypeCount = reader.readUInt32();
    header.timestampUs = reader.readUInt64();
    header.schemaVersion = reader.readUInt32();

    if (header.magic != SnapshotHeader::MAGIC) return result;

    result.reserve(header.entityCount);

    for (uint32_t a = 0; a < header.archetypeCount; ++a) {
        // Archetype-Hash wurde aus dem Wire-Format entfernt (Phase 0 Cleanup)
        uint32_t compCount = reader.readUInt32();
        uint32_t numEntities = reader.readUInt32();

        std::vector<seed::ecs::ComponentType> types;
        types.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            types.push_back(reader.readUInt32());
        }

        // GAP-FIX (2026-07-22, P1-3 Schema-Migration): gespeicherte Groessen
        // lesen (koennen kleiner sein als die aktuell registrierte Groesse,
        // wenn seither additiv Felder hinzugekommen sind).
        std::vector<uint32_t> storedSizes;
        storedSizes.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            storedSizes.push_back(reader.readUInt32());
        }

        // Cache meta sizes before entity loop
        std::vector<size_t> metaSizes;
        metaSizes.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(types[c]);
            SEED_ASSERT(meta.size > 0, "Snapshot::parseEntities: unknown component type");
            SEED_ASSERT(storedSizes[c] <= meta.size,
                        "Snapshot::parseEntities: stored component larger than current schema (Removal in Phase 0 nicht unterstuetzt)");
            metaSizes.push_back(storedSizes[c]); // beim Lesen die GESPEICHERTE Groesse nutzen, nicht die aktuelle
        }

        for (uint32_t e = 0; e < numEntities; ++e) {
            EntityState state;
            state.entity = reader.readUInt32();
            state.types = types;
            state.componentData.reserve(compCount);

            for (uint32_t c = 0; c < compCount; ++c) {
                std::vector<uint8_t> buffer(metaSizes[c]);
                reader.readBytes(buffer.data(), metaSizes[c]);
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
    SnapshotHeader header;
    header.magic = reader.readUInt32();
    header.version = reader.readUInt32();
    header.entityCount = reader.readUInt32();
    header.archetypeCount = reader.readUInt32();
    header.timestampUs = reader.readUInt64();
    header.schemaVersion = reader.readUInt32();
    return header.timestampUs;
}

void Snapshot::apply(seed::ecs::World& world) const {
    SEED_ZONE("Snapshot::apply");
    SEED_ASSERT(!m_data.empty(), "Cannot apply empty snapshot");

    BinaryReader reader(m_data);
    SnapshotHeader header;
    header.magic = reader.readUInt32();
    header.version = reader.readUInt32();
    header.entityCount = reader.readUInt32();
    header.archetypeCount = reader.readUInt32();
    header.timestampUs = reader.readUInt64();
    header.schemaVersion = reader.readUInt32();

    SEED_ASSERT(header.magic == SnapshotHeader::MAGIC, "Invalid snapshot magic");
    SEED_ASSERT(header.version == SnapshotHeader::VERSION, "Unsupported snapshot version");

    for (uint32_t a = 0; a < header.archetypeCount; ++a) {
        // Archetype-Hash wurde aus dem Wire-Format entfernt (Phase 0 Cleanup)

        uint32_t compCount = reader.readUInt32();
        uint32_t numEntities = reader.readUInt32();

        std::vector<seed::ecs::ComponentType> types;
        types.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            types.push_back(reader.readUInt32());
        }

        // GAP-FIX (2026-07-22, P1-3 Schema-Migration): gespeicherte Groessen
        // lesen, BEVOR types sortiert wird - die Reihenfolge im Stream
        // entspricht der Schreibreihenfolge in Snapshot::capture() (per
        // ComponentType gemappt, damit die Zuordnung auch nach dem
        // std::sort weiter unten garantiert korrekt bleibt).
        std::unordered_map<seed::ecs::ComponentType, uint32_t> storedSizeByType;
        storedSizeByType.reserve(compCount);
        for (uint32_t c = 0; c < compCount; ++c) {
            storedSizeByType[types[c]] = reader.readUInt32();
        }

        std::sort(types.begin(), types.end());

        // Cache meta sizes before entity loop; reuse a single buffer
        std::vector<size_t> metaSizes;   // Zielgroessen (aktuell registriert)
        std::vector<uint32_t> storedSizes; // GAP-FIX: tatsaechlich im Stream vorhandene Bytes
        metaSizes.reserve(compCount);
        storedSizes.reserve(compCount);
        size_t maxCompSize = 0;
        for (uint32_t c = 0; c < compCount; ++c) {
            const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(types[c]);
            SEED_ASSERT(meta.size > 0, "Snapshot::apply: unknown component type");
            uint32_t stored = storedSizeByType.at(types[c]);
            SEED_ASSERT(stored <= meta.size,
                        "Snapshot::apply: stored component larger than current schema (Removal in Phase 0 nicht unterstuetzt)");
            metaSizes.push_back(meta.size);
            storedSizes.push_back(stored);
            if (meta.size > maxCompSize) maxCompSize = meta.size;
        }

        std::vector<uint8_t> compBuffer;
        compBuffer.reserve(maxCompSize);

        for (uint32_t e = 0; e < numEntities; ++e) {
            seed::ecs::Entity storedEntity = reader.readUInt32();
            seed::ecs::Entity newEntity = world.createEntityWithId(storedEntity);

            for (uint32_t c = 0; c < compCount; ++c) {
                // GAP-FIX: erst auf die AKTUELLE (ggf. groessere) Zielgroesse
                // nullen, dann nur die GESPEICHERTE (ggf. kleinere) Menge
                // einlesen - additiv neue Felder bleiben so 0/Default.
                compBuffer.assign(metaSizes[c], 0);
                reader.readBytes(compBuffer.data(), storedSizes[c]);
                world.addComponentRaw(newEntity, types[c], compBuffer.data());
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
    oldMap.reserve(oldEntities.size() * 2);
    for (size_t i = 0; i < oldEntities.size(); ++i) {
        oldMap[oldEntities[i].entity] = i;
    }

    std::unordered_map<seed::ecs::Entity, size_t> newMap;
    newMap.reserve(newEntities.size() * 2);
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

    if (newEntities.size() > 4 && totalDeltaEntities > newEntities.size() / 2) {
        header.flags = 0x1;
        header.numChangedEntities = 0;
        header.numNewEntities = 0;
        header.numRemovedEntities = 0;
        // GAP-FIX (Cross-Platform-Determinismus): writePOD auf SnapshotHeader
    // schreibt auch Struct-Padding (4 Bytes nach schemaVersion), die
    // uninitialisierten Stack-Werte enthalten. Feld-fuer-Feld schreibt
    // exakt 28 Bytes, padding-frei, plattformidentisch.
    writer.writeUInt32(header.magic);
    writer.writeUInt32(header.version);
    writer.writeUInt32(header.entityCount);
    writer.writeUInt32(header.archetypeCount);
    writer.writeUInt64(header.timestampUs);
    writer.writeUInt32(header.schemaVersion);
        writer.writeUInt32(static_cast<uint32_t>(m_data.size()));
        writer.writeBytes(m_data.data(), m_data.size());
        return Delta(writer.data());
    }

    header.flags = 0x0;
    header.numChangedEntities = changedEntities;
    header.numNewEntities = newEntitiesCount;
    header.numRemovedEntities = removedEntitiesCount;
    // GAP-FIX (Cross-Platform-Determinismus): writePOD auf SnapshotHeader
    // schreibt auch Struct-Padding (4 Bytes nach schemaVersion), die
    // uninitialisierten Stack-Werte enthalten. Feld-fuer-Feld schreibt
    // exakt 28 Bytes, padding-frei, plattformidentisch.
    writer.writeUInt32(header.magic);
    writer.writeUInt32(header.version);
    writer.writeUInt32(header.entityCount);
    writer.writeUInt32(header.archetypeCount);
    writer.writeUInt64(header.timestampUs);
    writer.writeUInt32(header.schemaVersion);

    // Phase 1: changed entities (must match Delta::apply read order)
    for (const auto& newState : newEntities) {
        auto it = oldMap.find(newState.entity);
        if (it == oldMap.end()) continue; // skip new entities here

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
                if (oldCompIdx == static_cast<size_t>(-1) ||
                    oldState.componentData[oldCompIdx].size() != newState.componentData[c].size() ||
                    std::memcmp(oldState.componentData[oldCompIdx].data(),
                               newState.componentData[c].data(),
                               newState.componentData[c].size()) != 0) {
                    hasChanges = true;
                    break;
                }
            }
        }
        if (!hasChanges) continue;

        writer.writeUInt32(newState.entity);
        writer.writeUInt32(static_cast<uint32_t>(newState.types.size()));
        for (size_t c = 0; c < newState.types.size(); ++c) {
            size_t oldCompIdx = static_cast<size_t>(-1);
            for (size_t oc = 0; oc < oldState.types.size(); ++oc) {
                if (oldState.types[oc] == newState.types[c]) {
                    oldCompIdx = oc;
                    break;
                }
            }

            writer.writeUInt32(newState.types[c]);

            uint32_t compFlags = 0;
            std::vector<uint8_t> compData;
            const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(newState.types[c]);

            if (oldCompIdx != static_cast<size_t>(-1) &&
                oldState.componentData[oldCompIdx].size() == newState.componentData[c].size() &&
                meta.floatCount > 0 &&
                meta.size == sizeof(float) * meta.floatCount) {
                compFlags = 0x1;
                compData = seed::serialize::DeltaCompressor::compressFloatArray(
                    reinterpret_cast<const float*>(oldState.componentData[oldCompIdx].data()),
                    reinterpret_cast<const float*>(newState.componentData[c].data()),
                    meta.floatCount);
            } else if (oldCompIdx != static_cast<size_t>(-1) && meta.compress != nullptr) {
                compFlags = 0x2;
                compData = meta.compress(
                    oldState.componentData[oldCompIdx].data(),
                    newState.componentData[c].data(),
                    newState.componentData[c].size());
            } else {
                compFlags = 0x0;
                compData = newState.componentData[c];
            }

            writer.writeUInt32(compFlags);
            writer.writeUInt32(static_cast<uint32_t>(compData.size()));
            writer.writeBytes(compData.data(), compData.size());
        }
    }

    // Phase 2: removed entities
    for (const auto& oldState : oldEntities) {
        if (newMap.find(oldState.entity) == newMap.end()) {
            writer.writeUInt32(oldState.entity);
            writer.writeUInt32(0);
        }
    }

    // Phase 3: new entities
    for (const auto& newState : newEntities) {
        auto it = oldMap.find(newState.entity);
        if (it != oldMap.end()) continue; // skip existing entities here

        writer.writeUInt32(newState.entity);
        writer.writeUInt32(static_cast<uint32_t>(newState.types.size()));
        for (size_t c = 0; c < newState.types.size(); ++c) {
            writer.writeUInt32(newState.types[c]);
            writer.writeUInt32(0x0); // compFlags: raw
            writer.writeUInt32(static_cast<uint32_t>(newState.componentData[c].size()));
            writer.writeBytes(newState.componentData[c].data(), newState.componentData[c].size());
        }
    }

    return Delta(writer.data());
}

} // namespace seed::serialize
