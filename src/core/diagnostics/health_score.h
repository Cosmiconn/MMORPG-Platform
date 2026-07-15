#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace seed::diagnostics {

class HealthScore {
public:
    enum class Module : uint8_t {
        ECS = 0,
        Memory,
        Renderer,
        Jobs,
        Networking,
        Count
    };

    void setScore(Module mod, uint8_t score);
    uint8_t getScore(Module mod) const;
    bool isHealthy() const noexcept;
    Module worstModule() const;
    std::string report() const;

private:
    std::unordered_map<Module, uint8_t> m_scores;
};

} // namespace seed::diagnostics
