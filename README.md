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
| M2 | Custom Memory Management | ⏳ Next |
| M3 | ECS-Kern | 🔜 |
| M4 | Job-System | 🔜 |
| M5 | Serialisierung & Reflection | 🔜 |
| M6 | Profiler, Logger, Crash-Handler | 🔜 |
