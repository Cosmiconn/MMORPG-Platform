#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/health_score.h"
#include "core/diagnostics/ecs_validator.h"
#include "core/diagnostics/snapshot_dump.h"
#include <memory>
#include <string>
#include <fstream>

namespace seed::diagnostics {

class DiagnosticsManager {
public:
    static DiagnosticsManager& instance() noexcept;

    void initialize();
    void update();   // per frame
    void shutdown();

    EventTimeline& timeline() noexcept { return globalTimeline(); }
    HealthScore& health() noexcept { return m_health; }

    bool isHealthy() const noexcept;
    std::string fullReport() const;
    std::string fullReportJson() const;

    void snapshot(const seed::ecs::World& world, const char* reason = "manual");

    void setEcsValidationEnabled(bool enabled) noexcept { m_ecsValidation = enabled; }
    void setMemoryValidationEnabled(bool enabled) noexcept { m_memoryValidation = enabled; }
    bool ecsValidationEnabled() const noexcept { return m_ecsValidation; }
    bool memoryValidationEnabled() const noexcept { return m_memoryValidation; }

    // File logging
    bool openLogFile(const std::string& path);
    void closeLogFile();
    bool isLogFileOpen() const noexcept { return m_logFile.is_open(); }
    void writeToLog(const std::string& msg);
    bool writeToFile(const std::string& path) const;

    // Flush all buffered data
    void flush();

    // Tracy integration
    void tracyPlotEvent(const char* name, double value) const;

private:
    DiagnosticsManager() = default;

    HealthScore m_health;
    bool m_ecsValidation = SEED_DIAGNOSTICS_ECS_VALIDATION != 0;
    bool m_memoryValidation = SEED_DIAGNOSTICS_MEMORY_VALIDATION != 0;
    bool m_initialized = false;
    std::ofstream m_logFile;
    std::string m_logPath;
};

} // namespace seed::diagnostics
