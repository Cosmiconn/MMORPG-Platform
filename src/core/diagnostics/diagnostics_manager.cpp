#include "core/diagnostics/diagnostics_manager.h"
#include <fmt/format.h>
#include <fstream>
#include <chrono>

#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
#endif

namespace seed::diagnostics {

DiagnosticsManager& DiagnosticsManager::instance() noexcept {
    static DiagnosticsManager s_instance;
    return s_instance;
}

void DiagnosticsManager::initialize() {
    m_initialized = true;
    globalTimeline().clear();
    m_health.setScore(HealthScore::Module::ECS, 100);
    m_health.setScore(HealthScore::Module::Memory, 100);

    SEED_DIAG_EVENT(EventType::Custom, seed::ecs::INVALID_ENTITY, 0, 0, 0,
        "DiagnosticsManager initialized", __FILE__, __LINE__);

    writeToLog(fmt::format("[INIT] DiagnosticsManager initialized at {}\n", 
        SEED_BUILD_TIMESTAMP));
}

void DiagnosticsManager::update() {
    if (!m_initialized) return;

    // Periodic health check based on timeline event rates
    auto moveErrors = globalTimeline().getEventsByType(EventType::MoveOnlyError);
    auto asserts = globalTimeline().getEventsByType(EventType::AssertionFail);
    auto invariants = globalTimeline().getEventsByType(EventType::InvariantFail);

    if (!moveErrors.empty() || !asserts.empty() || !invariants.empty()) {
        m_health.setScore(HealthScore::Module::ECS, 10);
    } else if (globalTimeline().size() > SEED_DIAGNOSTICS_MAX_EVENTS * 0.8) {
        m_health.setScore(HealthScore::Module::ECS, 50);
    }
}

void DiagnosticsManager::shutdown() {
    if (m_logFile.is_open()) {
        m_logFile << "[SHUTDOWN] DiagnosticsManager shutdown\n";
        m_logFile.flush();
    }
    m_initialized = false;
}

bool DiagnosticsManager::isHealthy() const noexcept {
    return m_health.isHealthy();
}

std::string DiagnosticsManager::fullReport() const {
    std::string out;
    out += fmt::format("=== TheSeed Diagnostics Report ===\n");
    out += fmt::format("Build: {} | {} | {}\n", SEED_GIT_COMMIT, SEED_BUILD_TYPE, SEED_PLATFORM);
    out += fmt::format("Timestamp: {}\n", SEED_BUILD_TIMESTAMP);
    out += m_health.report();
    out += "\n";
    out += "=== Event Timeline ===\n";
    out += globalTimeline().dump();
    out += "\n";
    out += globalTimeline().performanceReport();
    return out;
}

std::string DiagnosticsManager::fullReportJson() const {
    std::string json = "{\n";
    json += fmt::format("  \"git_commit\": \"{}\",\n", SEED_GIT_COMMIT);
    json += fmt::format("  \"git_describe\": \"{}\",\n", SEED_GIT_DESCRIBE);
    json += fmt::format("  \"build_type\": \"{}\",\n", SEED_BUILD_TYPE);
    json += fmt::format("  \"compiler\": \"{}\",\n", SEED_COMPILER_INFO);
    json += fmt::format("  \"platform\": \"{}\",\n", SEED_PLATFORM);
    json += fmt::format("  \"build_timestamp\": \"{}\",\n", SEED_BUILD_TIMESTAMP);
    json += fmt::format("  \"healthy\": {},\n", isHealthy() ? "true" : "false");
    json += fmt::format("  \"timeline_size\": {},\n", globalTimeline().size());
    json += "  \"events\": [\n";

    auto events = globalTimeline().getEventsByType(EventType::Custom);
    // Would iterate all events here in full implementation
    json += "  ],\n";
    json += "  \"health\": " + m_health.report() + "\n";
    json += "}\n";
    return json;
}

void DiagnosticsManager::snapshot(const seed::ecs::World& world, const char* reason) {
    SnapshotDump dump;
    dump.capture(world);
    dump.errors.push_back(fmt::format("Manual snapshot: {}", reason));
    auto report = dump.toString();
    fmt::print("{}", report);
    writeToLog(report);
}

bool DiagnosticsManager::openLogFile(const std::string& path) {
    closeLogFile();
    m_logFile.open(path, std::ios::out | std::ios::app);
    if (m_logFile.is_open()) {
        m_logPath = path;
        m_logFile << fmt::format("[OPEN] Log opened at {} for build {}\n",
            SEED_BUILD_TIMESTAMP, SEED_GIT_COMMIT);
        return true;
    }
    return false;
}

void DiagnosticsManager::closeLogFile() {
    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    }
}

void DiagnosticsManager::writeToLog(const std::string& msg) {
    if (m_logFile.is_open()) {
        m_logFile << msg;
        if (msg.find('\n') == std::string::npos) {
            m_logFile << '\n';
        }
    }
}

bool DiagnosticsManager::writeToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << fullReport();
    return f.good();
}

void DiagnosticsManager::flush() {
    if (m_logFile.is_open()) {
        m_logFile.flush();
    }
}

void DiagnosticsManager::tracyPlotEvent(const char* name, double value) const {
#ifdef TRACY_ENABLE
    TracyPlot(name, value);
#else
    (void)name;
    (void)value;
#endif
}

} // namespace seed::diagnostics
