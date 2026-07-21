#include "core/profiling/frame_timer.h"
#include <algorithm>
#include <numeric>

namespace seed {

void FrameTimer::beginFrame() {
    m_frameStart = std::chrono::high_resolution_clock::now();
}

void FrameTimer::endFrame() {
    auto now = std::chrono::high_resolution_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(now - m_frameStart).count();

    m_current.deltaTime = elapsedMs;
    m_history.push_back(m_current);
    if (m_history.size() > MAX_HISTORY) {
        m_history.pop_front();
    }
}

float FrameTimer::averageFps() const {
    if (m_history.empty()) return 0.0f;
    float avgMs = std::accumulate(m_history.begin(), m_history.end(), 0.0f,
        [](float sum, const FrameStats& s) { return sum + s.deltaTime; }) / static_cast<float>(m_history.size());
    return avgMs > 0.0f ? 1000.0f / avgMs : 0.0f;
}

float FrameTimer::percentileFrameTime(float p) const {
    if (m_history.empty()) return 0.0f;
    std::vector<float> times;
    times.reserve(m_history.size());
    for (const auto& s : m_history) times.push_back(s.deltaTime);
    std::sort(times.begin(), times.end());
    size_t idx = static_cast<size_t>(p * static_cast<float>(times.size() - 1));
    return times[idx];
}

bool FrameTimer::isOverBudget(float budgetMs) const {
    return m_current.deltaTime > budgetMs;
}

const FrameStats& FrameTimer::getCurrentStats() const {
    return m_current;
}

} // namespace seed
