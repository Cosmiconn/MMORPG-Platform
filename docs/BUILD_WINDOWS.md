# TheSeed – Lokale Windows-Build-Anleitung (Visual Studio 2022/2026)

> **Voraussetzungen:** Visual Studio 2022/2026 mit C++-Workload, CMake 3.25+, Ninja, Git, vcpkg

---

## 1. Voraussetzungen prüfen

Öffne **Developer PowerShell for VS 2022** (oder VS 2026):
```powershell
# CMake Version (muss >= 3.25)
cmake --version

# Ninja
ninja --version

# Git
git --version

# MSVC Compiler
cl.exe
```

Falls etwas fehlt:
- **Visual Studio Installer** → "Modify" → Workloads → **"Desktop development with C++"**
- Optional: **"C++ CMake tools for Windows"**

---

## 2. vcpkg einrichten (einmalig)

```powershell
# Empfohlen: vcpkg neben dem Repo
C:\> cd C:\
C:\> git clone https://github.com/Microsoft/vcpkg.git
C:\> cd vcpkg
C:\vcpkg> .\bootstrap-vcpkg.bat

# Umgebungsvariable setzen (permanent über Systemeigenschaften oder temporär)
$env:VCPKG_ROOT = "C:\vcpkg"
# ODER permanent:
# [System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
```

---

## 3. Repo klonen / wechseln

```powershell
cd C:\dev  # oder dein Workspace
# Falls du das Repo bereits hast:
cd MMORPG-Platform
git fetch origin
git checkout feature/m02-memory

# Oder frisch klonen:
git clone --branch feature/m02-memory https://github.com/Cosmiconn/MMORPG-Platform.git
cd MMORPG-Platform
```

---

## 4. Build – Release (schnell, ohne Tests)

```powershell
# Umgebungsvariable setzen (falls nicht permanent)
$env:VCPKG_ROOT = "C:\vcpkg"

# Preset konfigurieren
cmake --preset windows-release

# Build
cmake --build build/windows-release --config Release --parallel

# Ergebnis:
# build/windows-release/Release/seed_smoke.exe
```

---

## 5. Build – Debug + Tests + ASan (empfohlen für Entwicklung)

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"

# 1. Konfigurieren (mit Sanitizers und Tests)
cmake --preset windows-debug

# 2. Build
cmake --build build/windows-debug --config Debug --parallel

# 3. Tests ausführen
ctest --test-dir build/windows-debug -C Debug --output-on-failure

# 4. Einzelnen Test wiederholen (z.B. der kritische Multi-Thread-Test)
ctest --test-dir build/windows-debug -C Debug -R Integration_MultiThreadStress --output-on-failure

# 5. Alle Memory-Tests 10x hintereinander (Stabilitätscheck)
for ($i = 1; $i -le 10; $i++) {
    Write-Host "=== Run $i ==="
    ctest --test-dir build/windows-debug -C Debug -R Integration_MultiThreadStress --output-on-failure
    if ($LASTEXITCODE -ne 0) { break }
}
```

**Wichtig bei MSVC ASan:**
- Die ASan-Runtime-DLLs (`clang_rt.asan_dbg_dynamic-x86_64.dll` etc.) werden von CMake/VS automatisch in das Build-Verzeichnis kopiert.
- Falls du Tests direkt ausführst (nicht über `ctest`), stelle sicher, dass diese DLLs im PATH sind oder im gleichen Ordner wie die `.exe` liegen.

---

## 6. Tracy Profiler aktivieren

Tracy wird über vcpkg installiert. Um es zu aktivieren:

```powershell
# Stelle sicher, dass Tracy in vcpkg installiert ist
& "$env:VCPKG_ROOT\vcpkg.exe" install tracy --triplet=x64-windows

# Tracy-Server bauen (optional, für GUI-Visualisierung)
cd "$env:VCPKG_ROOT\buildtrees\tracy\src\..."
# Oder lade Tracy-Server von https://github.com/wolfpld/tracy/releases

# Build mit Tracy-Client
cmake --preset windows-debug
# "Tracy found" sollte im Configure-Log erscheinen
cmake --build build/windows-debug --config Debug --parallel

# Tracy-Server starten und dann die App ausführen:
.\build\windows-debug\Debug\seed_smoke.exe
# In Tracy-Server: Connect to localhost (oder die IP des Rechners)
```

**Tracy-Makros die jetzt aktiv sind:**
- `SEED_ZONE("Name")` – erscheint in der Tracy-Timeline
- `SEED_ALLOC(ptr, size)` – Memory-Allokationen in der Heatmap
- `SEED_FRAME_MARK()` – Frame-Boundaries für Frame-Time-Analyse

---

## 7. Statische Analyse (clang-tidy / cppcheck)

```powershell
# clang-tidy (falls installiert via LLVM oder VS-Workload)
Get-ChildItem -Recurse -Filter *.cpp -Path src,tests | ForEach-Object {
    clang-tidy $_.FullName -- -I src
}

# cppcheck (falls installiert)
cppcheck --enable=all --error-exitcode=1 --suppress=missingIncludeSystem src/ tests/
```

---

## 8. Troubleshooting

### "Tracy not found" bei Configure
```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install tracy --triplet=x64-windows
# Dann cmake --preset erneut ausführen
```

### "ASan runtime not found" bei Test-Ausführung
Die ASan-DLLs müssen im PATH sein. Finde sie:
```powershell
Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\" -Recurse -Filter "*asan*.dll"
# Füge den Ordner zum PATH hinzu:
$env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64"
```

### vcpkg-Cache zu groß / langsam
```powershell
# vcpkg binary caching aktivieren
$env:VCPKG_BINARY_SOURCES = "clear;x-gha,readwrite"
# ODER lokal:
$env:VCPKG_BINARY_SOURCES = "clear;files,C:\vcpkg-cache,readwrite"
```

### Tests hängen / dauern ewig
Der `Integration_MultiThreadStress` mit 8 Threads × 50.000 Ops ist intensiv. Bei langsamer CPU kann das >10s dauern. Warte ab oder reduziere `OPS` im Test temporär.

---

## 9. Schnell-Checkliste vor Push

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"

# 1. Clean build
Remove-Item -Recurse -Force build/windows-debug -ErrorAction SilentlyContinue
cmake --preset windows-debug
cmake --build build/windows-debug --config Debug --parallel

# 2. Alle Tests
ctest --test-dir build/windows-debug -C Debug --output-on-failure
# Erwartet: 36/36 passed

# 3. Multi-Thread-Stress 10x
for ($i = 1; $i -le 10; $i++) {
    ctest --test-dir build/windows-debug -C Debug -R Integration_MultiThreadStress --output-on-failure
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED on run $i"; break }
}

# 4. Tracy-Server prüfen (optional)
# Starte Tracy-Server, führe seed_smoke.exe aus, prüfe ob FrameMark und Pool-Zonen sichtbar sind
```

---

**Bei Problemen:** Prüfe die CI-Logs auf GitHub (`.github/workflows/ci.yml`) – die Schritte dort sind identisch zu den lokalen Befehlen oben.
