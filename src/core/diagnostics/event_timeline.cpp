#include "core/diagnostics/event_timeline.h"
#include <chrono>
#include <fmt/format.h>
#include <algorithm>
#include <unordered_map>

namespace seed::diagnostics {

EventTimeline::EventTimeline() noexcept {
    m_buffer.fill(DiagnosticEvent{});
}

void EventTimeline::push(EventType type,
                         seed::ecs::Entity entity,
                         uint32_t archetypeHash,
                         uint32_t componentType,
                         uint32_t index,
                         const char* description,
                         const char* file,
                         int line,
                         uint64_t durationNs,
                         uint64_t memoryBytes) noexcept {
    const size_t idx = m_writeIdx.fetch_add(1, std::memory_order_relaxed) % Capacity;
    auto& slot = m_buffer[idx];

    slot.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    slot.frame = 0;
    slot.type = type;
    slot.entity = entity;
    slot.archetypeHash = archetypeHash;
    slot.componentType = componentType;
    slot.index = index;
    slot.description = description;
    slot.file = file;
    slot.line = line;
    slot.durationNs = durationNs;
    slot.memoryBytes = memoryBytes;

    // Tracy plot for performance events
    if (durationNs > 0) {
        tracyPlot("EventDuration_us", durationNs / 1000.0);
    }
    if (memoryBytes > 0) {
        tracyPlot("MemoryBytes", static_cast<double>(memoryBytes));
    }

    // File logging
    if (m_fileLoggingEnabled) {
        std::lock_guard<std::mutex> lock(m_logMutex);
        if (m_logFile.is_open()) {
            m_logFile << fmt::format("[{}] {} | E{:08x} | A{:08x} | C{} | I{} | {} | {}:{} | {}ns | {}B\n",
                eventTypeToString(type), entity, archetypeHash, componentType, index,
                description, file ? file : "?", line, durationNs, memoryBytes);
        }
    }
}

void EventTimeline::push(const DiagnosticEvent& ev) noexcept {
    const size_t idx = m_writeIdx.fetch_add(1, std::memory_order_relaxed) % Capacity;
    m_buffer[idx] = ev;
}

template<typename OutputIt>
void EventTimeline::drain(OutputIt out) noexcept {
    size_t r = m_readIdx.load(std::memory_order_relaxed);
    size_t w = m_writeIdx.load(std::memory_order_acquire);
    for (; r < w; ++r) {
        *out++ = m_buffer[r % Capacity];
    }
    m_readIdx.store(w, std::memory_order_release);
}

size_t EventTimeline::size() const noexcept {
    return m_writeIdx.load(std::memory_order_acquire) - m_readIdx.load(std::memory_order_relaxed);
}

void EventTimeline::clear() noexcept {
    m_readIdx.store(0, std::memory_order_relaxed);
    m_writeIdx.store(0, std::memory_order_release);
}

std::string EventTimeline::dump() const {
    std::string out;
    size_t r = m_readIdx.load(std::memory_order_relaxed);
    size_t w = m_writeIdx.load(std::memory_order_acquire);
    const size_t count = std::min(w - r, Capacity);

    for (size_t i = 0; i < count; ++i) {
        const auto& ev = m_buffer[(r + i) % Capacity];
        out += fmt::format("[{}] {} | E{:08x} | A{:08x} | C{} | I{} | {} | {}:{} | {}ns | {}B\n",
            i, eventTypeToString(ev.type), ev.entity, ev.archetypeHash,
            ev.componentType, ev.index, ev.description,
            ev.file ? ev.file : "?", ev.line, ev.durationNs, ev.memoryBytes);
    }
    return out;
}

std::vector<DiagnosticEvent> EventTimeline::getEventsByType(EventType type) const {
    std::vector<DiagnosticEvent> result;
    size_t r = m_readIdx.load(std::memory_order_relaxed);
    size_t w = m_writeIdx.load(std::memory_order_acquire);
    const size_t count = std::min(w - r, Capacity);

    for (size_t i = 0; i < count; ++i) {
        const auto& ev = m_buffer[(r + i) % Capacity];
        if (ev.type == type) result.push_back(ev);
    }
    return result;
}

std::string EventTimeline::performanceReport() const {
    std::string out = "=== Performance Report ===\n";

    struct PerfStats { uint64_t totalNs = 0; uint64_t count = 0; uint64_t maxNs = 0; uint64_t totalBytes = 0; };
    std::unordered_map<EventType, PerfStats> stats;

    size_t r = m_readIdx.load(std::memory_order_relaxed);
    size_t w = m_writeIdx.load(std::memory_order_acquire);
    const size_t count = std::min(w - r, Capacity);

    for (size_t i = 0; i < count; ++i) {
        const auto& ev = m_buffer[(r + i) % Capacity];
        if (ev.durationNs > 0 || ev.memoryBytes > 0) {
            auto& s = stats[ev.type];
            s.totalNs += ev.durationNs;
            s.count++;
            s.maxNs = std::max(s.maxNs, ev.durationNs);
            s.totalBytes += ev.memoryBytes;
        }
    }

    for (const auto& [type, s] : stats) {
        out += fmt::format("  {}: {} ops, avg {:.2f}us, max {:.2f}us, total {}B\n",
            eventTypeToString(type), s.count,
            s.count > 0 ? (s.totalNs / 1000.0) / s.count : 0.0,
            s.maxNs / 1000.0, s.totalBytes);
    }
    return out;
}

void EventTimeline::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logFilePath = path;
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_logFile.open(path, std::ios::out | std::ios::app);
    m_fileLoggingEnabled = m_logFile.is_open();
}

void EventTimeline::flushToFile() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile.is_open()) {
        m_logFile.flush();
    }
}

void EventTimeline::tracyPlot(const char* name, double value) const {
    #ifdef TRACY_ENABLE
        TracyPlot(name, value);
    #else
        (void)name; (void)value;
    #endif
}

EventTimeline& globalTimeline() noexcept {
    static EventTimeline s_instance;
    return s_instance;
}

template void EventTimeline::drain<std::back_insert_iterator<std::vector<DiagnosticEvent>>>(
    std::back_insert_iterator<std::vector<DiagnosticEvent>>) noexcept;

} // namespace seed::diagnostics
