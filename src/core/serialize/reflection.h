#pragma once

#include <cstddef>
#include <cstdint>
#include <typeinfo>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace seed::serialize {

class BinaryWriter;
class BinaryReader;

struct FieldInfo {
    std::string name;
    size_t offset;
    size_t size;
    std::string typeName;
};

struct TypeInfo {
    uint32_t typeId;
    std::string name;
    size_t size;
    size_t alignment;
    uint32_t version;
    std::vector<FieldInfo> fields;
};

template<typename T>
struct Reflect;

class TypeRegistry {
public:
    static TypeRegistry& instance();

    template<typename T>
    void registerType();

    const TypeInfo* getType(uint32_t typeId) const;
    const TypeInfo* getType(const std::string& name) const;
    bool isRegistered(uint32_t typeId) const;

    void serializeType(uint32_t typeId, BinaryWriter& writer) const;
    bool deserializeType(BinaryReader& reader);

    bool needsMigration(uint32_t typeId, uint32_t loadedVersion) const;

    using MigrateFunc = void(*)(void* oldData, size_t oldSize, void* newData, size_t newSize);
    void registerMigration(uint32_t typeId, uint32_t fromVersion, uint32_t toVersion, MigrateFunc func);
    bool migrate(uint32_t typeId, uint32_t fromVersion, uint32_t toVersion, void* oldData, size_t oldSize, void* newData, size_t newSize) const;

private:
    TypeRegistry() = default;
    std::unordered_map<uint32_t, TypeInfo> m_typesById;
    std::unordered_map<std::string, uint32_t> m_nameToId;
    std::unordered_map<uint32_t, std::unordered_map<uint64_t, MigrateFunc>> m_migrations;
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

    constexpr size_t fieldCount = std::tuple_size_v<decltype(R::fields)>;
    info.fields.reserve(fieldCount);
    std::apply([&info](auto&&... fields) {
        (info.fields.push_back(fields), ...);
    }, R::fields);

    m_typesById[info.typeId] = std::move(info);
    m_nameToId[std::string(R::name)] = R::typeId;
}

#define SEED_REFLECT_STRUCT(T, TYPEID, ...)     template<> struct seed::serialize::Reflect<T> {         using Self = T;         static constexpr uint32_t typeId = (TYPEID);         static constexpr const char* name = #T;         static constexpr size_t size = sizeof(T);         static constexpr size_t alignment = alignof(T);         static constexpr uint32_t version = 1;         inline static const auto fields = std::make_tuple(__VA_ARGS__);     };

#define SEED_FIELD(name)     seed::serialize::FieldInfo{ #name, offsetof(Self, name), sizeof(Self::name), "auto" }

template<typename T, typename = void>
struct has_reflect : std::false_type {};

template<typename T>
struct has_reflect<T, std::void_t<decltype(Reflect<T>::fields)>> : std::true_type {};

template<typename T>
inline constexpr bool has_reflect_v = has_reflect<T>::value;

} // namespace seed::serialize
