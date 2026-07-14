#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/health_score.h"
#include "core/diagnostics/ecs_validator.h"
#include "core/diagnostics/snapshot_dump.h"
#include <memory>

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// DiagnosticsManager – central hub for all diagnostic subsystems (TEDF)
// ---------------------------------------------------------------------------
// Usage:
//   auto& diag = DiagnosticsManager::instance();
//   diag.initialize();
//   diag.update(); // per frame
//   diag.shutdown();
// ---------------------------------------------------------------------------
class DiagnosticsManager {
public:
    static DiagnosticsManager& instance() noexcept;

    // Lifecycle
    void initialize();
    void update();   // call once per frame
    void shutdown();

    // Access subsystems
    EventTimeline& timeline() noexcept { return globalTimeline(); }
    HealthScore& health() noexcept { return m_health; }

    // Quick checks
    bool isHealthy() const noexcept;
    std::string fullReport() const;

    // Trigger manual snapshot
    void snapshot(const seed::ecs::World& world, const char* reason = "manual");

    // Configuration
    void setEcsValidationEnabled(bool enabled) noexcept { m_ecsValidation = enabled; }
    void setMemoryValidationEnabled(bool enabled) noexcept { m_memoryValidation = enabled; }
    bool ecsValidationEnabled() const noexcept { return m_ecsValidation; }
    bool memoryValidationEnabled() const noexcept { return m_memoryValidation; }

private:
    DiagnosticsManager() = default;

    HealthScore   m_health;
    bool          m_ecsValidation = SEED_DIAGNOSTICS_ECS_VALIDATION != 0;
    bool          m_memoryValidation = SEED_DIAGNOSTICS_MEMORY_VALIDATION != 0;
    bool          m_initialized = false;
};

} // namespace seed::diagnostics
