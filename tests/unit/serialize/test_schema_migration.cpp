#include <doctest/doctest.h>
#include "core/serialize/reflection.h"
#include "core/serialize/binary_writer.h"
#include "core/serialize/binary_reader.h"

using namespace seed::serialize;

struct OldData {
    float x;
    uint32_t y;
};

struct NewData {
    float x;
    uint32_t y;
    float z;
};

SEED_REFLECT_STRUCT(OldData, 1001, SEED_FIELD(x), SEED_FIELD(y))
SEED_REFLECT_STRUCT(NewData, 1001, SEED_FIELD(x), SEED_FIELD(y), SEED_FIELD(z))

TEST_SUITE("test_schema_migration") {

TEST_CASE("SchemaMigration_DetectVersionMismatch") {
    TypeRegistry::instance().registerType<NewData>();

    BinaryWriter writer;
    writer.writeUInt32(1001);
    writer.writeString("OldData");
    writer.writeUInt32(sizeof(OldData));
    writer.writeUInt32(alignof(OldData));
    writer.writeUInt32(1);
    writer.writeUInt32(2);

    writer.writeString("x");
    writer.writeUInt32(offsetof(OldData, x));
    writer.writeUInt32(sizeof(OldData::x));
    writer.writeString("float");

    writer.writeString("y");
    writer.writeUInt32(offsetof(OldData, y));
    writer.writeUInt32(sizeof(OldData::y));
    writer.writeString("uint32_t");

    BinaryReader reader(writer.data());
    bool ok = TypeRegistry::instance().deserializeType(reader);
    REQUIRE(ok);
    REQUIRE(TypeRegistry::instance().needsMigration(1001, 1) == true);
}

TEST_CASE("SchemaMigration_AdditiveField_DefaultInitialized") {
    TypeRegistry::instance().registerType<NewData>();
    REQUIRE(TypeRegistry::instance().getType(1001) != nullptr);
    REQUIRE(TypeRegistry::instance().getType(1001)->version == 1);
}

} // TEST_SUITE
