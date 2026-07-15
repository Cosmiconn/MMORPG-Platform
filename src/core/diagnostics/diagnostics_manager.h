#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/health_score.h"
#include "core/diagnostics/ecs_validator.h"
#include "core/diagnostics/snapshot_dump.h"
#include <memory>
#include <string>

namespace seed::diagnostics {

class DiagnosticsManager {
public:
    static DiagnosticsManager& instance() noexcept;

    void initialize();
    void update();
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

    bool writeToFile(const std::string& path) const;

private:
    DiagnosticsManager() = default;

    HealthScore m_health;
    bool m_ecsValidation = SEED_DIAGNOSTICS_ECS_VALIDATION != 0;
    bool m_memoryValidation = SEED_DIAGNOSTICS_MEMORY_VALIDATION != 0;
    bool m_initialized = false;
};

} // namespace seed::diagnostics
