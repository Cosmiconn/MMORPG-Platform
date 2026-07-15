#include "core/diagnostics/diagnostics_manager.h"
#include <fmt/format.h>
#include <fstream>

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
}

void DiagnosticsManager::update() {
    if (!m_initialized) return;
}

void DiagnosticsManager::shutdown() {
    m_initialized = false;
}

bool DiagnosticsManager::isHealthy() const noexcept {
    return m_health.isHealthy();
}

std::string DiagnosticsManager::fullReport() const {
    std::string out;
    out += m_health.report();
    out += "\n";
    out += "=== Event Timeline (last 50) ===\n";
    out += globalTimeline().dump();
    out += "\n";
    out += globalTimeline().performanceReport();
    return out;
}

std::string DiagnosticsManager::fullReportJson() const {
    std::string json = "{\n";
    json += "  \"healthy\": " + std::string(isHealthy() ? "true" : "false") + ",\n";
    json += "  \"timeline_size\": " + std::to_string(globalTimeline().size()) + "\n";
    json += "}\n";
    return json;
}

void DiagnosticsManager::snapshot(const seed::ecs::World& world, const char* reason) {
    SnapshotDump dump;
    dump.capture(world);
    dump.errors.push_back(fmt::format("Manual snapshot: {}", reason));
    fmt::print("{}", dump.toString());
}

bool DiagnosticsManager::writeToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << fullReport();
    return f.good();
}

} // namespace seed::diagnostics
