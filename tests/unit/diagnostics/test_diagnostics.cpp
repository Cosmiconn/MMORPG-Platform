#include <doctest/doctest.h>
#include "core/diagnostics/diagnostics_manager.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/health_score.h"

using namespace seed::diagnostics;

TEST_CASE("Diagnostics_Manager_Lifecycle") {
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    CHECK(diag.isHealthy());
    diag.shutdown();
}

TEST_CASE("HealthScore_SetGet") {
    HealthScore health;
    health.setScore(HealthScore::Module::ECS, 75);
    CHECK(health.getScore(HealthScore::Module::ECS) == 75);
}

TEST_CASE("EventTimeline_BasicPush") {
    EventTimeline timeline;
    timeline.clear();
    timeline.push(EventType::Custom, seed::ecs::INVALID_ENTITY, 0, 0, 0, "test", __FILE__, __LINE__);
    CHECK(timeline.size() == 1);
}
