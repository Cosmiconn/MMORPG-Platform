# Changelog – Monat 1: Build-System & CI/CD

## Deliverables
- `CMakeLists.txt` (Root) mit C++20, vcpkg-Integration
- `CMakePresets.json` für Windows (MSVC) und Linux (GCC)
- `vcpkg.json` Manifest mit doctest, fmt, spdlog, nlohmann-json, tracy
- `.github/workflows/ci.yml` – Build + Test + Sanitizer auf Push/PR
- `scripts/build.sh`, `scripts/build.bat`
- `tests/unit/main.cpp` (doctest)

## Acceptance Criteria Status
| Kriterium | Status |
|-----------|--------|
| `cmake --preset` Windows/Linux | ✅ |
| Clean-Build < 3min (nach vcpkg-Cache) | ✅ |
| CI: Build + Test + clang-tidy | ✅ |
| `-Wall -Wextra -Werror` clean | ✅ |
| C++20 nutzbar | ✅ |
