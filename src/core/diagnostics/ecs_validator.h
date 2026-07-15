#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include <string>
#include <vector>

namespace seed::ecs { class World; class Archetype; }

namespace seed::diagnostics {

class EcsValidator {
public:
    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        void addError(const std::string& msg) {
            valid = false;
            errors.push_back(msg);
        }
        void addWarning(const std::string& msg) {
            warnings.push_back(msg);
        }
    };

    ValidationResult validateWorld(const seed::ecs::World& world) const;
    ValidationResult validateArchetype(const seed::ecs::Archetype& arch) const;
    ValidationResult validateWorldDetailed(seed::ecs::World& world, const char* file, int line);
    std::string fullReport(const seed::ecs::World& world) const;
};

#define SEED_VALIDATE_ECS(world) \
    do { \
        if (seed::diagnostics::EcsValidator().validateWorld(world).valid == false) { \
            SEED_DIAG_EVENT(seed::diagnostics::EventType::InvariantFail, \
                seed::ecs::INVALID_ENTITY, 0, 0, 0, \
                "ECS validation failed", __FILE__, __LINE__); \
        } \
    } while(0)

} // namespace seed::diagnostics
