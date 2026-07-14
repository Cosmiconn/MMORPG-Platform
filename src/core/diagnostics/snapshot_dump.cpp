#include "core/diagnostics/snapshot_dump.h"
#include "core/diagnostics/health_score.h"
#include "core/ecs/world.h"
#include <fmt/format.h>
#include <fstream>
#include <chrono>

#if defined(__clang__)
#  define SEED_COMPILER "clang " __clang_version__
#elif defined(__GNUC__)
#  define SEED_COMPILER "gcc " __VERSION__
#elif defined(_MSC_VER)
#  define SEED_COMPILER "msvc " _MSC_VER
#else
#  define SEED_COMPILER "unknown"
#endif

#if defined(__linux__)
#  define SEED_PLATFORM "linux"
#elif defined(_WIN32)
#  define SEED_PLATFORM "windows"
#else
#  define SEED_PLATFORM "unknown"
#endif


// Assert hook for SEED_ASSERT integration
namespace seed {
    void (*g_seed_assert_hook)(const char* reason, const char* f, int l) = nullptr;
}

namespace seed::diagnostics {

void SnapshotDump::capture(const seed::ecs::World& world) {
    captureBuildInfo();
    captureHealth();
    captureTimeline();

    // Capture world dump
    // Note: World::dump() prints to stdout via fmt::print.
    // For snapshot we need string capture. We use a custom formatter.
    worldDump = "=== World Snapshot ===\n";
    worldDump += fmt::format("Entity count: {}\n", world.entityCount());
    // TODO: extend World with toString() for proper snapshot capture
}

void SnapshotDump::captureTimeline() {
    eventTimeline = globalTimeline().dump();
}

void SnapshotDump::captureHealth() {
    healthReport = HealthScore::report();
}

void SnapshotDump::captureBuildInfo() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char timeStr[64] = {};
    #ifdef _WIN32
        struct tm tm_info;
        localtime_s(&tm_info, &time_t);
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);
    #else
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
    #endif

    #if SEED_DIAGNOSTICS_ENABLED
        const char* diagStatus = "enabled";
    #else
        const char* diagStatus = "disabled";
    #endif

    buildInfo = std::string("Build Info:\n")
        + "  Compiler: " + std::string(SEED_COMPILER) + "\n"
        + "  Platform: " + std::string(SEED_PLATFORM) + "\n"
        + "  Time:     " + std::string(timeStr) + "\n"
        + "  Diagnostics: " + std::string(diagStatus) + "\n";
}

std::string SnapshotDump::toString() const {
    std::string out;
    out.reserve(4096);
    out += buildInfo;
    out += "\n";
    out += healthReport;
    out += "\n";
    out += worldDump;
    out += "\n";
    out += "=== Event Timeline ===\n";
    out += eventTimeline;
    out += "\n";
    if (!errors.empty()) {
        out += "=== Errors ===\n";
        for (const auto& err : errors) {
            out += err;
            out += "\n";
        }
    }
    return out;
}

bool SnapshotDump::writeToFile(const std::string& path) const {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << toString();
    return ofs.good();
}

// ---------------------------------------------------------------------------
// SnapshotOnFailure
// ---------------------------------------------------------------------------

void SnapshotOnFailure::install() {
    // Install signal handlers for SIGSEGV, SIGABRT, etc.
    // TODO: implement platform-specific signal handling

    // Register assert hook
    ::seed::g_seed_assert_hook = [](const char* reason, const char* f, int l) {
        trigger(reason, f, l, nullptr);
    };
}

void SnapshotOnFailure::trigger(const char* reason,
                                const char* file,
                                int line,
                                const seed::ecs::World* world) {
    SnapshotDump dump;
    if (world) {
        dump.capture(*world);
    } else {
        dump.captureBuildInfo();
        dump.captureHealth();
        dump.captureTimeline();
    }
    dump.errors.push_back(fmt::format("{} at {}:{}", reason, file, line));
    writeEmergencyDump(dump);
}

void SnapshotOnFailure::writeEmergencyDump(const SnapshotDump& dump) {
    const std::string path = "seed_crash_dump.txt";
    if (dump.writeToFile(path)) {
        fmt::print(stderr, "\n!!! CRASH DUMP WRITTEN TO: {} !!!\n\n", path);
    } else {
        fmt::print(stderr, "\n!!! FAILED TO WRITE CRASH DUMP !!!\n");
        fmt::print(stderr, "{}", dump.toString());
    }
}

} // namespace seed::diagnostics
