#pragma once

#include "core/diagnostics/event_timeline.h"

namespace seed::profiling {

// Install custom sanitizer error handlers that log to diagnostics
void installSanitizerHooks();

// These are called by the sanitizer runtime
extern "C" {
    // AddressSanitizer
    void __asan_on_error();
    void __asan_report_error(const char* file, int line, const char* func,
                             const char* msg);

    // UndefinedBehaviorSanitizer
    void __ubsan_handle_type_mismatch();
    void __ubsan_handle_alignment_assumption();
}

} // namespace seed::profiling
