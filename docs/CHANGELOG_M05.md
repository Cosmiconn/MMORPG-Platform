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

## Known Gaps (siehe `docs/PHASE0_TEST_QUALITY.md`)
- Component-aware Delta-Kompression (XOR-Float nicht in Snapshot-Delta integriert)
- Performance-Assertions für 100k Entities fehlen in CI
- Schema-Migration ist Stub (Erkennung vorhanden, keine Transformation)
- `string_view`-Lifetime in Reflection theoretisch sicher (String-Literal), aber nicht explizit

## Acceptance Criteria Status
| Kriterium | Status |
|-----------|--------|
| 100k Snapshot <100ms | ⚠️ Manuell |
| Snapshot <50MB | ⚠️ Manuell |
| Delta 10MB→<100KB | ❌ Nicht optimal |
| Deserialize <50ms | ❌ Nicht getestet |
| Schema-Versioning | ⚠️ Stub |
| Cross-Platform Bytes | ✅ |
| ASan clean | ✅ |
