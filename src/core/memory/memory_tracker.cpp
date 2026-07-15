#include "core/memory/memory_tracker.h"
#include <fmt/format.h>

namespace seed::memory {

void MemoryTracker::trackAllocation(const std::string& category, size_t size) {
    m_stats.totalAllocated += size;
    m_stats.totalUsed += size;
    m_stats.activeAllocations++;

    size_t current = m_stats.totalUsed.load();
    size_t peak = m_stats.peakUsed.load();
    while (current > peak && !m_stats.peakUsed.compare_exchange_weak(peak, current)) {
        peak = m_stats.peakUsed.load();
    }

    auto& budget = m_budgets[category];
    budget.totalAllocated += size;
    budget.totalUsed += size;
    budget.activeAllocations++;
}

void MemoryTracker::trackDeallocation(const std::string& category, size_t size) {
    m_stats.totalUsed -= size;
    m_stats.activeAllocations--;

    auto& budget = m_budgets[category];
    budget.totalUsed -= size;
    budget.activeAllocations--;
}

bool MemoryTracker::checkBudget(const std::string& category) const {
    auto it = m_budgets.find(category);
    if (it == m_budgets.end()) return true;
    // Budget logic would go here
    return true;
}

std::string MemoryTracker::report() const {
    std::string out = "=== Memory Tracker ===\n";
    out += fmt::format("Total allocated: {} bytes\n", m_stats.totalAllocated.load());
    out += fmt::format("Total used: {} bytes\n", m_stats.totalUsed.load());
    out += fmt::format("Peak used: {} bytes\n", m_stats.peakUsed.load());
    out += fmt::format("Active allocations: {}\n", m_stats.activeAllocations.load());
    return out;
}

} // namespace seed::memory
