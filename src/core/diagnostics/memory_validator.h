#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include <cstddef>
#include <string>

namespace seed::memory { class Allocator; }

namespace seed::diagnostics {

class MemoryValidator {
public:
    struct ValidationResult {
        bool valid = true;
        std::string error;

        void setError(const std::string& msg) {
            valid = false;
            error = msg;
        }
    };

    ValidationResult validateAllocator(const seed::memory::Allocator& alloc) const;
    ValidationResult validatePointer(const void* ptr, size_t size, size_t alignment) const;

    // Track allocation lifecycle
    void trackAllocate(const void* ptr, size_t size, const char* category);
    void trackDeallocate(const void* ptr, size_t size);

    bool isTracked(const void* ptr) const;
    size_t getAllocatedSize(const void* ptr) const;

    std::string fullReport() const;
};

// Macros for automatic tracking
#if SEED_DIAGNOSTICS_MEMORY_VALIDATION
#  define SEED_TRACK_ALLOC(ptr, size, cat) \
     ::seed::diagnostics::MemoryValidator().trackAllocate(ptr, size, cat)
#  define SEED_TRACK_DEALLOC(ptr, size) \
     ::seed::diagnostics::MemoryValidator().trackDeallocate(ptr, size)
#else
#  define SEED_TRACK_ALLOC(ptr, size, cat) ((void)0)
#  define SEED_TRACK_DEALLOC(ptr, size) ((void)0)
#endif

} // namespace seed::diagnostics
