@echo off
set PRESET=%1
if "%PRESET%"=="" set PRESET=windows-release
cmake --preset %PRESET%
cmake --build --preset %PRESET% --parallel
if "%PRESET%"=="windows-debug" ctest --preset %PRESET% --output-on-failure
