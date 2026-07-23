# Profiling & Observability (Monat 6)

## Komponenten
- `FrameTimer` – Frame-Time-Tracking, Budget-Alarm, P99
- `CrashHandler` – Signal-Handler, Stack-Traces, Minidumps (Windows)
- `Profiler` – Tracy-Wrapper Makros
- `LogSystem` – spdlog mit Rotating-File-Sink

## Nutzung

```cpp
// In main()
seed::CrashHandler::install();
seed::log::LogSystem::instance().initialize("logs/seed.log");

// Per Frame
frameTimer.beginFrame();
// ... game loop ...
frameTimer.endFrame();
if (frameTimer.isOverBudget()) {
    SEED_LOG_WARN("Frame over budget", delta_ms=frameTimer.getCurrentStats().deltaTime);
}

// Profiling
void someSystem() {
    SEED_PROFILE_ZONE("someSystem");
    // ...
}
```

## Acceptance Criteria (Gate 0)
- [ ] 100k Entities, 24h, 0 Leaks
- [ ] Serialisierung 100k in <10ms
- [ ] Tracy zeigt Frame-Times, System-Times, Memory, Jobs
- [ ] Frame-Budget-Alarm >16.67ms → Warn-Log
- [ ] Crash-Test `*(int*)0=0` → Minidump + Log
- [ ] Log-Rotation: 100MB, max 5 Dateien
