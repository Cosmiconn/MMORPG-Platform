#include "core/diagnostics/health_score.h"
#include <fmt/format.h>
#include <algorithm>

namespace seed::diagnostics {

void HealthScore::setScore(Module mod, uint8_t score) {
    m_scores[mod] = std::min(score, static_cast<uint8_t>(100));
}

uint8_t HealthScore::getScore(Module mod) const {
    auto it = m_scores.find(mod);
    return (it != m_scores.end()) ? it->second : 100;
}

bool HealthScore::isHealthy() const noexcept {
    for (const auto& [mod, score] : m_scores) {
        if (score < 50) return false;
    }
    return true;
}

HealthScore::Module HealthScore::worstModule() const {
    Module worst = Module::ECS;
    uint8_t minScore = 100;
    for (const auto& [mod, score] : m_scores) {
        if (score < minScore) {
            minScore = score;
            worst = mod;
        }
    }
    return worst;
}

std::string HealthScore::report() const {
    std::string out = "=== Health Score ===\n";
    for (const auto& [mod, score] : m_scores) {
        const char* name = "Unknown";
        switch (mod) {
            case Module::ECS: name = "ECS"; break;
            case Module::Memory: name = "Memory"; break;
            case Module::Renderer: name = "Renderer"; break;
            case Module::Jobs: name = "Jobs"; break;
            case Module::Networking: name = "Networking"; break;
            default: break;
        }
        out += fmt::format("  {}: {}/100\n", name, score);
    }
    return out;
}

} // namespace seed::diagnostics
