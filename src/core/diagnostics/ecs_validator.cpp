#include "core/diagnostics/ecs_validator.h"
#include "core/ecs/world.h"
#include "core/ecs/archetype.h"
#include "core/diagnostics/event_timeline.h"
#include <fmt/format.h>

namespace seed::diagnostics {

EcsValidator::ValidationResult EcsValidator::validateWorld(const seed::ecs::World& world) const {
    ValidationResult result;

    size_t alive = 0;
    for (const auto& slot : world.getEntities()) {
        if (slot.alive) ++alive;
    }
    if (alive != world.aliveCount()) {
        result.addError(fmt::format("Alive count mismatch: counted {} vs tracked {}", 
            alive, world.aliveCount()));
    }

    for (size_t i = 0; i < world.getEntities().size(); ++i) {
        if (world.getEntities()[i].alive) {
            const auto& rec = world.getRecords()[i];
            if (rec.archetypeId.hash != 0) {
                auto* arch = world.getArchetype(rec.archetypeId);
                if (!arch) {
                    result.addError(fmt::format("Entity {} has invalid archetype", i));
                } else if (rec.index >= arch->size()) {
                    result.addError(fmt::format("Entity {} index {} out of range {}", 
                        i, rec.index, arch->size()));
                } else if (arch->entityAt(rec.index) != world.getEntities()[i].entity) {
                    result.addError(fmt::format("Entity {} mismatch at archetype index {}", 
                        i, rec.index));
                }
            }
        }
    }

    for (const auto& [id, arch] : world.getArchetypeManager()) {
        auto archResult = validateArchetype(*arch);
        if (!archResult.valid) {
            for (const auto& e : archResult.errors) {
                result.addError(fmt::format("Archetype {:08x}: {}", id.hash, e));
            }
        }
    }

    return result;
}

EcsValidator::ValidationResult EcsValidator::validateArchetype(const seed::ecs::Archetype& arch) const {
    ValidationResult result;

    size_t count = arch.size();
    for (const auto& col : arch.getColumns()) {
        if (col->size() != count) {
            result.addError(fmt::format("Column size {} != entity count {}", 
                col->size(), count));
        }
    }

    return result;
}

EcsValidator::ValidationResult EcsValidator::validateWorldDetailed(
    seed::ecs::World& world, const char* file, int line) {

    ValidationResult result = validateWorld(world);

    if (!result.valid) {
        for (const auto& err : result.errors) {
            SEED_DIAG_EVENT(EventType::InvariantFail, seed::ecs::INVALID_ENTITY, 
                0, 0, 0, err.c_str(), file, line);
        }
    }

    return result;
}

std::string EcsValidator::fullReport(const seed::ecs::World& world) const {
    auto result = validateWorld(world);
    std::string out = "=== ECS Validation Report ===\n";
    out += result.valid ? "VALID\n" : "INVALID\n";
    for (const auto& e : result.errors) out += "  ERROR: " + e + "\n";
    for (const auto& w : result.warnings) out += "  WARN:  " + w + "\n";
    return out;
}

} // namespace seed::diagnostics
