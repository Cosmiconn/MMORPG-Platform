#include "core/diagnostics/memory_validator.h"
#include "core/memory/allocator.h"
#include "core/diagnostics/event_timeline.h"
#include <fmt/format.h>
#include <unordered_map>
#include <mutex>

namespace seed::diagnostics {

struct AllocRecord {
    size_t size;
    std::string category;
    uint64_t timestamp;
};

static std::unordered_map<const void*, AllocRecord> g_allocMap;
static std::mutex g_allocMutex;

MemoryValidator::ValidationResult MemoryValidator::validateAllocator(
    const seed::memory::Allocator& alloc) const {
    ValidationResult result;
    (void)alloc; // Would check allocator state in full implementation
    return result;
}

MemoryValidator::ValidationResult MemoryValidator::validatePointer(
    const void* ptr, size_t size, size_t alignment) const {
    ValidationResult result;
    if (!ptr) {
        result.setError("Null pointer");
        return result;
    }
    if (size == 0) {
        result.setError("Zero-size allocation");
        return result;
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        result.setError(fmt::format("Invalid alignment: {}", alignment));
        return result;
    }
    return result;
}

void MemoryValidator::trackAllocate(const void* ptr, size_t size, const char* category) {
    std::lock_guard<std::mutex> lock(g_allocMutex);
    g_allocMap[ptr] = {size, category ? category : "unknown", 
        std::chrono::steady_clock::now().time_since_epoch().count()};

    SEED_DIAG_EVENT(EventType::MemoryAllocate, seed::ecs::INVALID_ENTITY, 
        0, 0, 0, fmt::format("alloc {} bytes [{}]", size, category).c_str(), 
        __FILE__, __LINE__);
}

void MemoryValidator::trackDeallocate(const void* ptr, size_t size) {
    std::lock_guard<std::mutex> lock(g_allocMutex);
    auto it = g_allocMap.find(ptr);
    if (it != g_allocMap.end()) {
        if (it->second.size != size && size != 0) {
            SEED_DIAG_EVENT(EventType::InvariantFail, seed::ecs::INVALID_ENTITY,
                0, 0, 0, 
                fmt::format("Dealloc size mismatch: tracked {} vs requested {}", 
                    it->second.size, size).c_str(),
                __FILE__, __LINE__);
        }
        g_allocMap.erase(it);
    } else {
        SEED_DIAG_EVENT(EventType::InvariantFail, seed::ecs::INVALID_ENTITY,
            0, 0, 0, "Dealloc of untracked pointer", __FILE__, __LINE__);
    }

    SEED_DIAG_EVENT(EventType::MemoryDeallocate, seed::ecs::INVALID_ENTITY,
        0, 0, 0, fmt::format("dealloc {} bytes", size).c_str(), __FILE__, __LINE__);
}

bool MemoryValidator::isTracked(const void* ptr) const {
    std::lock_guard<std::mutex> lock(g_allocMutex);
    return g_allocMap.find(ptr) != g_allocMap.end();
}

size_t MemoryValidator::getAllocatedSize(const void* ptr) const {
    std::lock_guard<std::mutex> lock(g_allocMutex);
    auto it = g_allocMap.find(ptr);
    return (it != g_allocMap.end()) ? it->second.size : 0;
}

std::string MemoryValidator::fullReport() const {
    std::lock_guard<std::mutex> lock(g_allocMutex);
    std::string out = "=== Memory Validator Report ===\n";
    out += fmt::format("Active allocations: {}\n", g_allocMap.size());
    size_t totalBytes = 0;
    for (const auto& [ptr, rec] : g_allocMap) {
        totalBytes += rec.size;
        out += fmt::format("  {}: {} bytes [{}]\n", ptr, rec.size, rec.category);
    }
    out += fmt::format("Total tracked: {} bytes\n", totalBytes);
    return out;
}

} // namespace seed::diagnostics
