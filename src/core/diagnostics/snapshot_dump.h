#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include <string>
#include <vector>

namespace seed::ecs {
    class World;
}

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// SnapshotDump – captures ECS state for crash reproduction (TEDF Layer 6)
// ---------------------------------------------------------------------------
// Captures:
//   - All archetypes with component signatures and entity counts
//   - All entity records (index, version, archetype, row)
//   - Event timeline (last N events)
//   - Health scores
//   - Build info (git commit, compiler, platform)
// ---------------------------------------------------------------------------
struct SnapshotDump {
    std::string buildInfo;
    std::string healthReport;
    std::string eventTimeline;
    std::string worldDump;
    std::string archetypeDump;
    std::vector<std::string> errors;

    void capture(const seed::ecs::World& world);
    void captureTimeline();
    void captureHealth();
    void captureBuildInfo();

    std::string toString() const;
    bool writeToFile(const std::string& path) const;
};

// ---------------------------------------------------------------------------
// Crash handler integration – auto-dump on assertion failure
// ---------------------------------------------------------------------------
class SnapshotOnFailure {
public:
    static void install();
    static void trigger(const char* reason,
                        const char* file,
                        int line,
                        const seed::ecs::World* world = nullptr);

private:
    static void writeEmergencyDump(const SnapshotDump& dump);
};

#if SEED_DIAGNOSTICS_SNAPSHOT_ON_FAILURE
#  define SEED_SNAPSHOT_ON_FAIL(world, reason)      ::seed::diagnostics::SnapshotOnFailure::trigger((reason), __FILE__, __LINE__, &(world))
#else
#  define SEED_SNAPSHOT_ON_FAIL(world, reason) ((void)0)
#endif

} // namespace seed::diagnostics
