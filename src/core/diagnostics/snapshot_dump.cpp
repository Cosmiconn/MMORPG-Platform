#include "core/diagnostics/snapshot_dump.h"
#include "core/ecs/world.h"
#include <fmt/format.h>
#include <fstream>
#include <cstdlib>

// Build-time constants (should be defined via CMake)
#ifndef SEED_GIT_COMMIT
#define SEED_GIT_COMMIT "unknown"
#endif
#ifndef SEED_BUILD_TYPE
#define SEED_BUILD_TYPE "unknown"
#endif
#ifndef SEED_COMPILER_INFO
#define SEED_COMPILER_INFO "unknown"
#endif
#ifndef SEED_PLATFORM
#define SEED_PLATFORM "unknown"
#endif

namespace seed::diagnostics {

void SnapshotDump::capture(const seed::ecs::World& world) {
    captureTime = std::chrono::system_clock::now();

    buildInfo.gitCommit = SEED_GIT_COMMIT;
    buildInfo.buildType = SEED_BUILD_TYPE;
    buildInfo.compiler = SEED_COMPILER_INFO;
    buildInfo.platform = SEED_PLATFORM;
    buildInfo.timestamp = __DATE__ " " __TIME__;

    globalTimeline().drain(std::back_inserter(events));
    ecsState = world.dumpToString();
}

std::string SnapshotDump::toString() const {
    std::string out;
    out += "========================================\n";
    out += "  TheSeed Engine Diagnostics Dump\n";
    out += "========================================\n";
    out += fmt::format("Capture Time: {}\n", 
        std::chrono::system_clock::to_time_t(captureTime));
    out += fmt::format("Git Commit: {}\n", buildInfo.gitCommit);
    out += fmt::format("Build: {} | {} | {}\n", 
        buildInfo.buildType, buildInfo.compiler, buildInfo.platform);
    out += "\n--- Health Score ---\n";
    out += health.report();
    out += "\n--- Errors ---\n";
    for (const auto& e : errors) out += fmt::format("  ERROR: {}\n", e);
    out += "\n--- Warnings ---\n";
    for (const auto& w : warnings) out += fmt::format("  WARN:  {}\n", w);
    out += "\n--- Event Timeline ---\n";
    for (const auto& ev : events) {
        out += fmt::format("[{}] E{:08x} | {} | {} | {}us\n",
            eventTypeToString(ev.type),
            ev.entity,
            ev.description,
            ev.file,
            ev.durationNs / 1000);
    }
    out += "\n--- ECS State ---\n";
    out += ecsState;
    out += "========================================\n";
    return out;
}

std::string SnapshotDump::toJson() const {
    std::string json = "{\n";
    json += fmt::format("  \"git_commit\": \"{}\",\n", buildInfo.gitCommit);
    json += fmt::format("  \"build_type\": \"{}\",\n", buildInfo.buildType);
    json += fmt::format("  \"platform\": \"{}\",\n", buildInfo.platform);
    json += "  \"errors\": [\n";
    for (size_t i = 0; i < errors.size(); ++i) {
        json += fmt::format("    \"{}\"{}\n", 
            errors[i], i + 1 < errors.size() ? "," : "");
    }
    json += "  ],\n";
    json += "  \"warnings\": [\n";
    for (size_t i = 0; i < warnings.size(); ++i) {
        json += fmt::format("    \"{}\"{}\n", 
            warnings[i], i + 1 < warnings.size() ? "," : "");
    }
    json += "  ],\n";
    json += fmt::format("  \"event_count\": {},\n", events.size());
    json += fmt::format("  \"ecs_state\": \"{}\"\n", ecsState);
    json += "}\n";
    return json;
}

bool SnapshotDump::writeToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << toString();
    return f.good();
}

static std::string g_crashReason;

void installCrashHandler() {
    // Platform-specific crash handlers would go here
    (void)0;
}

void triggerDiagnosticsDump(const char* reason) {
    g_crashReason = reason;
    fmt::print(stderr, "\n[DIAGNOSTICS DUMP] {}\n", reason);
    SnapshotDump dump;
    fmt::print(stderr, "{}\n", dump.toString());
}

} // namespace seed::diagnostics
