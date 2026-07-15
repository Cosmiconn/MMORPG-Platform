#include "core/memory/memory_system.h"

namespace seed::memory {

MemorySystem& MemorySystem::instance() {
    static MemorySystem s_instance;
    return s_instance;
}

void MemorySystem::initialize() {
    m_tracker.trackAllocation("system", sizeof(MemorySystem));
}

void MemorySystem::shutdown() {
}

} // namespace seed::memory
