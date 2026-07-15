#pragma once

#if defined(_MSC_VER)
#pragma warning(disable: 4324)
#endif

#include "core/diagnostics/diagnostics_config.h"
#include "core/ecs/entity.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace seed::diagnostics {

enum class EventType : uint8_t {
    EntityCreate, EntityDestroy, EntityRecycle,
    ComponentAdd, ComponentRemove, ComponentMove,
    ComponentDestruct, ComponentDefaultConstruct,
    ArchetypeCreate, ArchetypeDestroy,
    MemoryAllocate, MemoryDeallocate,
    SystemUpdate, WorldValidate,
    InvariantFail, AssertionFail,
    MoveOnlyError, PerformanceWarning,
    Custom
};

inline const char* eventTypeToString(EventType t) noexcept {
    switch (t) {
        case EventType::EntityCreate: return "EntityCreate";
        case EventType::EntityDestroy: return "EntityDestroy";
        case EventType::EntityRecycle: return "EntityRecycle";
        case EventType::ComponentAdd: return "ComponentAdd";
        case EventType::ComponentRemove: return "ComponentRemove";
        case EventType::ComponentMove: return "ComponentMove";
        case EventType::ComponentDestruct: return "ComponentDestruct";
        case EventType::ComponentDefaultConstruct: return "ComponentDefaultConstruct";
        case EventType::ArchetypeCreate: return "ArchetypeCreate";
        case EventType::ArchetypeDestroy: return "ArchetypeDestroy";
        case EventType::MemoryAllocate: return "MemoryAllocate";
        case EventType::MemoryDeallocate: return "MemoryDeallocate";
        case EventType::SystemUpdate: return "SystemUpdate";
        case EventType::WorldValidate: return "WorldValidate";
        case EventType::InvariantFail: return "InvariantFail";
        case EventType::AssertionFail: return "AssertionFail";
        case EventType::MoveOnlyError: return "MoveOnlyError";
        case EventType::PerformanceWarning: return "PerformanceWarning";
        case EventType::Custom: return "Custom";
    }
    return "Unknown";
}

struct DiagnosticEvent {
    uint64_t        timestamp;
    uint32_t        frame;
    EventType       type;
    seed::ecs::Entity entity;
    uint32_t        archetypeHash;
    uint32_t        componentType;
    uint32_t        index;
    const char*     description;
    const char*     file;
    int             line;
    uint64_t        durationNs;

    DiagnosticEvent() noexcept
        : timestamp(0), frame(0), type(EventType::Custom)
        , entity(seed::ecs::INVALID_ENTITY), archetypeHash(0)
        , componentType(0), index(0), description(""), file(""), line(0)
        , durationNs(0)
    {}
};

class EventTimeline {
public:
    static constexpr size_t Capacity = SEED_DIAGNOSTICS_MAX_EVENTS;

    EventTimeline() noexcept;

    void push(EventType type,
              seed::ecs::Entity entity = seed::ecs::INVALID_ENTITY,
              uint32_t archetypeHash = 0,
              uint32_t componentType = 0,
              uint32_t index = 0,
              const char* description = "",
              const char* file = "",
              int line = 0,
              uint64_t durationNs = 0) noexcept;

    void push(const DiagnosticEvent& ev) noexcept;

    template<typename OutputIt>
    void drain(OutputIt out) noexcept;

    size_t size() const noexcept;
    void clear() noexcept;
    std::string dump() const;
    std::vector<DiagnosticEvent> getEventsByType(EventType type) const;
    std::string performanceReport() const;

private:
    alignas(64) std::array<DiagnosticEvent, Capacity> m_buffer;
    alignas(64) std::atomic<size_t> m_writeIdx{0};
    alignas(64) std::atomic<size_t> m_readIdx{0};
};

EventTimeline& globalTimeline() noexcept;

#if SEED_DIAGNOSTICS_EVENT_TIMELINE
#  define SEED_DIAG_EVENT(type, entity, arch, comp, idx, desc, file, line) \
     ::seed::diagnostics::globalTimeline().push((type), (entity), (arch), (comp), (idx), (desc), (file), (line))
#  define SEED_DIAG_EVENT_PERF(type, entity, arch, comp, idx, desc, file, line, ns) \
     ::seed::diagnostics::globalTimeline().push((type), (entity), (arch), (comp), (idx), (desc), (file), (line), (ns))
#else
#  define SEED_DIAG_EVENT(type, entity, arch, comp, idx, desc, file, line) ((void)0)
#  define SEED_DIAG_EVENT_PERF(type, entity, arch, comp, idx, desc, file, line, ns) ((void)0)
#endif

} // namespace seed::diagnostics
