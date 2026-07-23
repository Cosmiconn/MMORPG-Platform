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

const TypeInfo* TypeRegistry::getType(const std::string& name) const {
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
        writer.writeString(field.name);
        writer.writeUInt32(static_cast<uint32_t>(field.offset));
        writer.writeUInt32(static_cast<uint32_t>(field.size));
        writer.writeString(field.typeName);
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

    const TypeInfo* existing = getType(id);
    if (existing) {
        if (existing->version != version) {
            // Phase 0: additive changes only. In Phase 4+ this is where
            // migrate() would be called per-component during Snapshot::apply.
            // For now we accept the mismatch; ECS ComponentMeta::construct
            // zero-initializes new fields.
        }
        for (uint32_t i = 0; i < fieldCount; ++i) {
            (void)reader.readString();
            (void)reader.readUInt32();
            (void)reader.readUInt32();
            (void)reader.readString();
        }
        return true;
    }

    TypeInfo info;
    info.typeId = id;
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

    m_nameToId[name] = id;
    info.name = std::move(name);
    m_typesById[id] = std::move(info);
    return true;
}

bool TypeRegistry::needsMigration(uint32_t typeId, uint32_t loadedVersion) const {
    const TypeInfo* info = getType(typeId);
    if (!info) return false;
    return info->version != loadedVersion;
}

void TypeRegistry::registerMigration(uint32_t typeId, uint32_t fromVersion, uint32_t toVersion, MigrateFunc func) {
    uint64_t key = (static_cast<uint64_t>(fromVersion) << 32) | toVersion;
    m_migrations[typeId][key] = func;
}

bool TypeRegistry::migrate(uint32_t typeId, uint32_t fromVersion, uint32_t toVersion, void* oldData, size_t oldSize, void* newData, size_t newSize) const {
    uint64_t key = (static_cast<uint64_t>(fromVersion) << 32) | toVersion;
    auto itType = m_migrations.find(typeId);
    if (itType == m_migrations.end()) return false;
    auto itFunc = itType->second.find(key);
    if (itFunc == itType->second.end()) return false;
    itFunc->second(oldData, oldSize, newData, newSize);
    return true;
}

} // namespace seed::serialize
