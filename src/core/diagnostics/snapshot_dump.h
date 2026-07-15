#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/health_score.h"
#include <string>
#include <vector>
#include <chrono>

namespace seed::ecs { class World; }

namespace seed::diagnostics {

struct BuildInfo {
    std::string gitCommit;
    std::string buildType;
    std::string compiler;
    std::string platform;
    std::string timestamp;
};

struct SnapshotDump {
    BuildInfo buildInfo;
    HealthScore health;
    std::vector<DiagnosticEvent> events;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string ecsState;
    std::string memoryState;
    std::chrono::system_clock::time_point captureTime;

    void capture(const seed::ecs::World& world);
    std::string toString() const;
    std::string toJson() const;
    bool writeToFile(const std::string& path) const;
};

void installCrashHandler();
void triggerDiagnosticsDump(const char* reason);

} // namespace seed::diagnostics
