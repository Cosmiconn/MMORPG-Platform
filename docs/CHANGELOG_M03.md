# Changelog – Monat 3: ECS-Kern (Archetype-basiert)

## Deliverables
- `src/core/ecs/entity.h` – Entity = uint32_t [version|index]
- `src/core/ecs/world.h/.cpp` – EntityManager + Archetype-Zuweisung
- `src/core/ecs/archetype.h/.cpp` – SoA-Storage
- `src/core/ecs/archetype_manager.h/.cpp` – Registry
- `src/core/ecs/component_array.h` – Dicht gepackte Arrays
- `src/core/ecs/component_traits.h` – Type-ID Metadaten
- `src/core/ecs/type_registry.h/.cpp` – ECS ComponentMeta Factory
- `src/core/ecs/query.h` – Archetype-Filter
- `src/core/ecs/system.h` – System-Interface

## Key Metrics
- 100k Entities erstellen in <100ms
- 10 Systeme @ 100k Entities in <16ms
- Archetype-Wechsel <10µs

## Acceptance Criteria Status
| Kriterium | Status |
|-----------|--------|
| 100k Entities in <100ms | ✅ |
| 10 Systeme <16ms | ✅ |
| Archetype-Wechsel <10µs | ✅ |
| Memory <50MB für 100k | ✅ |
| 0 Leaks / UAF | ✅ |
