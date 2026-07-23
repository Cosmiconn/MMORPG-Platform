#pragma once
#include <cstdint>
#include <deque>
#include <chrono>

namespace seed {

struct FrameStats {
    float deltaTime = 0.0f;
    float cpuTime = 0.0f;
    uint32_t entityCount = 0;
    size_t memoryUsed = 0;
};

class FrameTimer {
public:
    void beginFrame();
    void endFrame();

    float averageFps() const;
    float percentileFrameTime(float p) const;
    bool isOverBudget(float budgetMs = 16.667f) const;
    const FrameStats& getCurrentStats() const;

private:
    std::chrono::high_resolution_clock::time_point m_frameStart;
    FrameStats m_current;
    std::deque<FrameStats> m_history;
    static constexpr size_t MAX_HISTORY = 300;
};

} // namespace seed
