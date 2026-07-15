#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace seed::memory {

struct MemoryStats {
    std::atomic<size_t> totalAllocated{0};
    std::atomic<size_t> totalUsed{0};
    std::atomic<size_t> peakUsed{0};
    std::atomic<uint32_t> activeAllocations{0};
};

class MemoryTracker {
public:
    void trackAllocation(const std::string& category, size_t size);
    void trackDeallocation(const std::string& category, size_t size);
    bool checkBudget(const std::string& category) const;

    size_t totalAllocated() const { return m_stats.totalAllocated.load(); }
    size_t peakUsed() const { return m_stats.peakUsed.load(); }

    std::string report() const;

private:
    MemoryStats m_stats;
    std::unordered_map<std::string, MemoryStats> m_budgets;
};

} // namespace seed::memory
