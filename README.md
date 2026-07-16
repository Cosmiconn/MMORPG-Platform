# TheSeed

Solo-to-AAA MMORPG Platform

## Status: Phase 0 – Fundament

| Monat | Thema | Status |
|-------|-------|--------|
| 1 | Build-System & CI/CD | ✅ Abgeschlossen |
| 2 | Custom Memory Management | ✅ Abgeschlossen |
| 3 | ECS-Kern (EnTT-Style) | ✅ Abgeschlossen |
| 4 | Job-System (Work-Stealing) | 🔄 In Progress |
| 5 | Serialisierung & Reflection | ⏳ Offen |
| 6 | Profiler, Logger, Crash-Handler | ⏳ Offen |

## Quick Start

```bash
# Configure (Linux)
cmake --preset linux-release

# Build
cmake --build build/release

# Test
ctest --test-dir build/release

# Run benchmarks
./build/release/tests/seed_bench_jobs
```

## Architecture

- **Memory:** Pool-, Arena-, Stack-Allocator + BlockAllocator (OS)
- **ECS:** Archetype-based, SoA layout, 100k entities @ 60 FPS
- **Jobs:** Lock-free work-stealing (Chase-Lev), DAG dependencies, parallelFor

## Dependencies

Managed via vcpkg:
- doctest, spdlog, fmt, nlohmann-json, tracy (optional)

## License

Proprietary – Cosmiconn
