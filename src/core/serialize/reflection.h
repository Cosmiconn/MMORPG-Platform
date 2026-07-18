#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <string>
#include <vector>

namespace seed::serialize {

class BinaryWriter;
class BinaryReader;

// ---------------------------------------------------------------------------
// Field description
// ---------------------------------------------------------------------------
struct FieldInfo {
    std::string_view name;
    size_t offset;
    size_t size;
    std::string_view typeName;
};

// ---------------------------------------------------------------------------
// Type description with schema versioning
// ---------------------------------------------------------------------------
struct TypeInfo {
    uint32_t typeId;
    std::string_view name;
    size_t size;
    size_t alignment;
    uint32_t version; // Schema version for migration
    std::vector<FieldInfo> fields;
};

// ---------------------------------------------------------------------------
// Reflect<T> – specialize per type
// ---------------------------------------------------------------------------
template<typename T>
struct Reflect;

// ---------------------------------------------------------------------------
// TypeRegistry – runtime type information for serialization
// ---------------------------------------------------------------------------
class TypeRegistry {
public:
    static TypeRegistry& instance();

    template<typename T>
    void registerType();

    const TypeInfo* getType(uint32_t typeId) const;
    const TypeInfo* getType(std::string_view name) const;
    bool isRegistered(uint32_t typeId) const;

    void serializeType(uint32_t typeId, BinaryWriter& writer) const;
    bool deserializeType(BinaryReader& reader);

    // Schema migration: check if loaded version differs from registered version
    bool needsMigration(uint32_t typeId, uint32_t loadedVersion) const;

private:
    TypeRegistry() = default;
    std::unordered_map<uint32_t, TypeInfo> m_typesById;
    std::unordered_map<std::string_view, uint32_t> m_nameToId;
};

template<typename T>
void TypeRegistry::registerType() {
    using R = Reflect<T>;
    TypeInfo info;
    info.typeId = R::typeId;
    info.name = R::name;
    info.size = R::size;
    info.alignment = R::alignment;
    info.version = R::version;

    // Unpack fields tuple into vector
    constexpr size_t fieldCount = std::tuple_size_v<decltype(R::fields)>;
    info.fields.reserve(fieldCount);
    std::apply([&info](auto&&... fields) {
        (info.fields.push_back(fields), ...);
    }, R::fields);

    m_typesById[info.typeId] = std::move(info);
    m_nameToId[R::name] = R::typeId;
}

// ---------------------------------------------------------------------------
// Helper macros
// ---------------------------------------------------------------------------
#define SEED_REFLECT_STRUCT(T, TYPEID, ...) \
    template<> struct seed::serialize::Reflect<T> { \
        using Self = T; \
        static constexpr uint32_t typeId = (TYPEID); \
        static constexpr std::string_view name = #T; \
        static constexpr size_t size = sizeof(T); \
        static constexpr size_t alignment = alignof(T); \
        static constexpr uint32_t version = 1; \
        static constexpr auto fields = std::make_tuple(__VA_ARGS__); \
    };

#define SEED_FIELD(name) \
    seed::serialize::FieldInfo{ #name, offsetof(Self, name), sizeof(Self::name), "auto" }

} // namespace seed::serialize
