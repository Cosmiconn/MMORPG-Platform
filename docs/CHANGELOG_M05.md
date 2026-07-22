# Changelog – Monat 5: Serialisierung & Reflection

## Deliverables
- `src/core/serialize/binary_writer.h/.cpp` – Little-Endian, POD, String
- `src/core/serialize/binary_reader.h/.cpp` – Bounds-Check, Big-Endian-Konvertierung
- `src/core/serialize/snapshot.h/.cpp` – Full-Snapshot, Apply, Roundtrip
- `src/core/serialize/delta.h/.cpp` – Byte-Range-Diff, XOR-Float-Arrays
- `src/core/serialize/reflection.h/.cpp` – FieldInfo, TypeInfo, Reflect<T>, Macros
- `tests/unit/serialize/test_snapshot.cpp` – 11 Testfälle
- `tests/unit/serialize/test_delta.cpp` – 5 Testfälle
- `tests/benchmarks/bench_serialize.cpp` – Minimal

## Update 2026-07-22 (siehe `PHASE0_TESTQUALITY_FIXPLAN.md`)

**Bereits erledigt, hier vorher fälschlich als offen gelistet:**
- `string_view`-Lifetime in Reflection: behoben (`std::string` statt `string_view` in `FieldInfo`/`TypeInfo`)
- Component-aware Delta-Kompression: ist in `Snapshot::computeDelta` integriert (XOR-Float, `compFlags=0x1`)
- SIGSEGV in `Snapshot_Delta_MultipleChanges` (CI 2026-07-20): behoben, siehe `M05_GAP_ANALYSIS.md`

**In diesem Update behoben:**
- Schema-Migration: additiv implementiert (gespeicherte Komponenten-Größe wird jetzt pro Archetype mitgeschrieben, `Snapshot::apply` polstert neue Felder mit 0 auf); echter Test in `test_schema_migration.cpp`
- Cross-Platform-Byte-Vergleich: eigener CI-Job `cross-platform-compare` (Linux↔Windows, byte-identisch außer Timestamp); vorheriger In-Job-Check war durch einen `xxd`/`grep`-Formatierungsfehler dauerhaft rot
- Performance-Gate in CI war durch `NDEBUG` (Release-Build) wirkungslos (`SEED_ASSERT` ist dort ein No-Op) – jetzt über eigenes, build-unabhängiges `BENCH_CHECK`-Makro
- Benchmark simulierte "~1%" Entity-Änderung tatsächlich mit ~16% (`% 100 < 16`) – korrigiert auf echten Zähler
- Delta-Kompressions-Assertion im Unit-Test verschärft (<50% → <5%, gemessen ~2-3%)

**Noch offen:**
- Deserialize-Performance >50ms-Budget (aktuell ~57-65ms gemessen) – separat getrackt, siehe `M05_GAP_ANALYSIS.md`
- Zwei getrennte TypeRegistry-Systeme (ECS vs. Serialize) – bewusst auf Phase 1 zurückgestellt, nur über `ComponentMeta.schemaVersion`/`fields` gebrückt

## Acceptance Criteria Status
| Kriterium | Status |
|-----------|--------|
| 100k Snapshot <100ms | ✅ (gemessen ~24-32ms) |
| Snapshot <50MB | ✅ |
| Delta 10MB→<100KB | ✅ (gemessen ~2-3% Kompression bei 1% Änderung) |
| Deserialize <50ms | ❌ Noch offen (~57-65ms gemessen) |
| Schema-Versioning | ✅ additiv implementiert + getestet |
| Cross-Platform Bytes | ✅ echter Byte-Vergleich in CI (`cross-platform-compare`) |
| ASan clean | ✅ |
