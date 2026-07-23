#include <doctest/doctest.h>
#include "core/serialize/reflection.h"
#include "core/serialize/binary_writer.h"
#include "core/serialize/binary_reader.h"
#include "core/serialize/snapshot.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/type_registry.h"
#include "core/memory/memory_system.h"

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

// GAP-FIX (2026-07-22, P1-3): Die beiden Tests oben pruefen nur rohes
// Bytes-Schreiben/Lesen bzw. dass zwei UNABHAENGIGE Structs unterschiedliche
// TypeInfo::version-Werte tragen - keiner von beiden geht durch die
// tatsaechliche Snapshot::apply()-Pipeline. Der folgende Test tut das:
// er baut per BinaryWriter von Hand einen "alten" Snapshot, dessen
// MigSnapPosition-Komponente nur 8 Byte (x,y) enthaelt, waehrend die
// AKTUELL registrierte MigSnapPosition 12 Byte (x,y,z) hat - genau der
// additive Migrationsfall aus der Monat-5-Spec ("alte Snapshots koennen in
// neue Welt geladen werden"). Zwei VERSCHIEDENE C++-Typen unter derselben
// ComponentType-ID zu registrieren ist von TypeRegistry::registerComponent()
// absichtlich blockiert (siehe core/ecs/type_registry.h) - deshalb der
// Umweg ueber einen handgebauten Snapshot statt einer zweiten Struct-Version.
struct MigSnapPosition { float x, y, z; }; // aktuell registriert: 12 Byte
SEED_REGISTER_COMPONENT_WITH_ID(MigSnapPosition, 200)

TEST_CASE("Snapshot_SchemaMigration_AdditiveField_RealPipeline") {
    using namespace seed::memory;
    using namespace seed::ecs;

    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    seed::ecs::TypeRegistry::instance().registerComponent<MigSnapPosition>();

    // Handgebauter "alter" Snapshot: MigSnapPosition hatte frueher nur x,y
    // (8 Byte gespeichert), aktuell ist die Komponente mit 12 Byte (x,y,z)
    // registriert. Format muss exakt Snapshot::capture() entsprechen:
    // Header, dann pro Archetype: compCount, numEntities, [ctype]*,
    // [storedSize]*, dann pro Entity: entity, [componentBytes]*.
    // NOTE: Archetype-Hash wurde aus dem Wire-Format entfernt (Phase 0 Cleanup).
    BinaryWriter w;
    SnapshotHeader header;
    header.entityCount = 1;
    header.archetypeCount = 1;
    w.writeUInt32(header.magic);
    w.writeUInt32(header.version);
    w.writeUInt32(header.entityCount);
    w.writeUInt32(header.archetypeCount);
    w.writeUInt64(header.timestampUs);
    w.writeUInt32(header.schemaVersion);

    w.writeUInt32(1);       // compCount = 1
    w.writeUInt32(1);       // numEntities = 1
    w.writeUInt32(200);     // ctype = MigSnapPosition
    w.writeUInt32(8);       // GESPEICHERTE Groesse: 8 Byte (altes 2-Feld-Format)
    w.writeUInt32(42);      // entity id
    w.writeFloat(1.5f);     // x
    w.writeFloat(2.5f);     // y
    // kein z im alten Format - muss nach dem Laden 0.0f sein

    Snapshot oldSnap = Snapshot::deserialize(w.data());

    World world(&blockAlloc);
    oldSnap.apply(world);

    auto* pos = world.getComponent<MigSnapPosition>(42u);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == doctest::Approx(1.5f));
    CHECK(pos->y == doctest::Approx(2.5f));
    CHECK(pos->z == doctest::Approx(0.0f)); // additiv neues Feld -> Default 0
}
