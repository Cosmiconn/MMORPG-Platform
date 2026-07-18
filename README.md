# TheSeed

> Solo-to-AAA MMORPG Platform — Phase 0: Fundament & Architektur

## Quick Start

### Prerequisites
- CMake 3.25+
- Ninja
- vcpkg (with `VCPKG_ROOT` environment variable set)
- C++20 compiler: MSVC 2022+ (Windows) / GCC 13+ (Linux)

### Linux
```bash
export VCPKG_ROOT=~/vcpkg
./scripts/build.sh linux-release
```

### Windows
```batch
set VCPKG_ROOT=C:cpkg
scripts\build.bat windows-release
```

### CMake Presets (direct)
```bash
cmake --preset linux-release
cmake --build build/linux-release --parallel
ctest --test-dir build/linux-release
```

## CI Status
- Linux (Ubuntu 24.04): Build + Test + Sanitizers + clang-tidy/cppcheck
- Windows (Windows-latest): Build + Test + Artifact upload

## Phase 0 Roadmap
| Month | Topic | Status |
|-------|-------|--------|
| M1 | Build-System & CI/CD | ✅ Gate ready |
| M2 | Custom Memory Management | ✅ Fertig (Block/Pool/Arena/Stack-Allocator, MemoryTracker) |
| M3 | ECS-Kern | ✅ Fertig (Archetype-ECS, Query, System) |
| M4 | Job-System | ✅ Fertig (Work-Stealing, Task-Graph/DAG, parallelFor) |
| M5 | Serialisierung & Reflection | 🔜 Nicht begonnen |
| M6 | Profiler, Logger, Crash-Handler | ⏳ Teilweise (EventTimeline, ECS-/Memory-Validator, HealthScore, Crash-Dump via Signal-Handler; FrameTimer und Logger-Wrapper (spdlog) noch offen) |

Hinweis: Diese Tabelle wurde am 2026-07-16 aktualisiert, nachdem sie laengere
Zeit hinter dem tatsaechlichen Code-Stand zuruecklag (M2/M3 waren schon lange
fertig, standen hier aber noch auf "Next"/"🔜"). Bitte nach jedem
abgeschlossenen Monat kurz aktualisieren, sonst verliert die Tabelle wieder
ihren Wert als Fortschritts-Uebersicht.
