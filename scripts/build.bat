@echo off
setlocal enabledelayedexpansion

:: TheSeed Build Script for Windows
:: Usage: scripts\build.bat [preset] [--test]
::   preset: windows-debug | windows-release (default: windows-release)

set "PRESET=%~1"
if "%~1"=="" set "PRESET=windows-release"
set "BUILD_DIR=build\%PRESET%"

echo === TheSeed Build ===
echo Preset: %PRESET%
echo Build dir: %BUILD_DIR%
echo.

:: ---------------------------------------------------------------------------
:: Prerequisites
:: ---------------------------------------------------------------------------
where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake not found ^(need ^>= 3.25^)
    exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo ERROR: ninja not found
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: vcpkg discovery
:: ---------------------------------------------------------------------------
if "%VCPKG_ROOT%"=="" (
    echo INFO: VCPKG_ROOT not set, probing default locations...
    if exist "C:cpkg" set "VCPKG_ROOT=C:cpkg"
    if exist "%USERPROFILE%cpkg" set "VCPKG_ROOT=%USERPROFILE%cpkg"
    if exist "%USERPROFILE%\sourcecpkg" set "VCPKG_ROOT=%USERPROFILE%\sourcecpkg"
)

if "%VCPKG_ROOT%"=="" (
    echo ERROR: vcpkg not found. Install vcpkg and set VCPKG_ROOT.
    exit /b 1
)

:: Export toolchain for CMakePresets.json
set "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scriptsuildsystemscpkg.cmake"

:: ---------------------------------------------------------------------------
:: Configure ^& Build
:: ---------------------------------------------------------------------------
echo --- Configuring ---
cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1

echo.
echo --- Building ---
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b 1

:: ---------------------------------------------------------------------------
:: Test (debug presets only, or if --test flag is passed)
:: ---------------------------------------------------------------------------
echo %PRESET% | findstr "debug" >nul
if not errorlevel 1 (
    echo.
    echo --- Testing ---
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
)
if "%~2"=="--test" (
    echo.
    echo --- Testing ---
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
)

echo.
echo === Build Complete ===
echo Binary: %BUILD_DIR%\seed_smoke.exe
