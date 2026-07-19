#include "core/serialize/reflection.h"
#include "core/serialize/binary_writer.h"
#include "core/profiling/seed_assert.h"
#include "core/serialize/binary_reader.h"

namespace seed::serialize {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

const TypeInfo* TypeRegistry::getType(uint32_t typeId) const {
    auto it = m_typesById.find(typeId);
    return (it != m_typesById.end()) ? &it->second : nullptr;
}

const TypeInfo* TypeRegistry::getType(std::string_view name) const {
    auto it = m_nameToId.find(name);
    if (it == m_nameToId.end()) return nullptr;
    return getType(it->second);
}

bool TypeRegistry::isRegistered(uint32_t typeId) const {
    return m_typesById.find(typeId) != m_typesById.end();
}

void TypeRegistry::serializeType(uint32_t typeId, BinaryWriter& writer) const {
    const TypeInfo* info = getType(typeId);
    SEED_ASSERT(info != nullptr, "TypeRegistry::serializeType: unknown type");

    writer.writeUInt32(info->typeId);
    writer.writeString(info->name);
    writer.writeUInt32(static_cast<uint32_t>(info->size));
    writer.writeUInt32(static_cast<uint32_t>(info->alignment));
    writer.writeUInt32(info->version);
    writer.writeUInt32(static_cast<uint32_t>(info->fields.size()));

    for (const auto& field : info->fields) {
        writer.writeString(std::string(field.name));
        writer.writeUInt32(static_cast<uint32_t>(field.offset));
        writer.writeUInt32(static_cast<uint32_t>(field.size));
        writer.writeString(std::string(field.typeName));
    }
}

bool TypeRegistry::deserializeType(BinaryReader& reader) {
    uint32_t id = reader.readUInt32();
    std::string name = reader.readString();
    uint32_t size = reader.readUInt32();
    uint32_t alignment = reader.readUInt32();
    uint32_t version = reader.readUInt32();
    uint32_t fieldCount = reader.readUInt32();

    (void)size;
    (void)alignment;

    // Check if we already know this type
    const TypeInfo* existing = getType(id);
    if (existing) {
        // Schema migration check
        if (existing->version != version) {
            // Minimal schema migration: additive fields are default-initialized.
            // Phase 0 only supports additive changes (new fields at end).
            // Removals / reorders require manual migration hooks (Phase 4+).
            if (version > existing->version) {
                // Upgrade path: new fields in info.fields not present in existing->fields
                // are silently accepted; deserialization will zero-initialize them.
                *existing = info;
            }
            // Downgrade: keep old schema (old snapshots loaded into newer world)
        }
        // Skip field data
        for (uint32_t i = 0; i < fieldCount; ++i) {
            (void)reader.readString(); // name
            (void)reader.readUInt32(); // offset
            (void)reader.readUInt32(); // size
            (void)reader.readString(); // typeName
        }
        return true;
    }

    // Unknown type: store minimal info for forward compatibility
    TypeInfo info;
    info.typeId = id;
    info.name = std::move(name);
    info.version = version;
    info.size = size;
    info.alignment = alignment;

    for (uint32_t i = 0; i < fieldCount; ++i) {
        FieldInfo f;
        f.name = reader.readString();
        f.offset = reader.readUInt32();
        f.size = reader.readUInt32();
        f.typeName = reader.readString();
        info.fields.push_back(std::move(f));
    }

    m_typesById[id] = std::move(info);
    m_nameToId[name] = id;
    return true;
}

bool TypeRegistry::needsMigration(uint32_t typeId, uint32_t loadedVersion) const {
    const TypeInfo* info = getType(typeId);
    if (!info) return false; // Unknown type, can't migrate
    return info->version != loadedVersion;
}

} // namespace seed::serialize
