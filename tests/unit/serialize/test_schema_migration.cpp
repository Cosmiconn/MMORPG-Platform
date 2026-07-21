#include <doctest/doctest.h>
#include "core/serialize/reflection.h"
#include "core/serialize/binary_writer.h"
#include "core/serialize/binary_reader.h"

using namespace seed::serialize;

// Schema v1
struct OldData {
    uint32_t id;
    float x;
};

// Schema v2 (additive field)
struct NewData {
    uint32_t id;
    float x;
    float y; // additive in v2
};

TEST_CASE("SchemaMigration_AdditiveField_DefaultInitialized") {
    // When deserializing old data into a struct that expects a new field,
    // the new field should be default-initialized.
    BinaryWriter writer;
    writer.writeUInt32(42);
    writer.writeFloat(3.14f);

    auto data = writer.data();
    BinaryReader reader(data);

    uint32_t id = reader.readUInt32();
    float x = reader.readFloat();

    CHECK(id == 42);
    CHECK(x == doctest::Approx(3.14f));
}

TEST_CASE("SchemaMigration_VersionMismatch_Detected") {
    TypeRegistry reg;
    reg.registerType<OldData>("OldData", 1);
    reg.registerType<NewData>("NewData", 2);

    auto* oldInfo = reg.getType("OldData");
    auto* newInfo = reg.getType("NewData");

    REQUIRE(oldInfo != nullptr);
    REQUIRE(newInfo != nullptr);
    CHECK(oldInfo->version == 1);
    CHECK(newInfo->version == 2);
    CHECK(oldInfo->version != newInfo->version);
}
