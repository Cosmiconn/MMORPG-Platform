#include "core/profiling/sanitizer_hooks.h"
#include "core/diagnostics/diagnostics_manager.h"
#include "core/diagnostics/snapshot_dump.h"
#include <fmt/format.h>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Sanitizer Integration
// 
// These hooks are called by ASan/UBSan when an error is detected.
// We capture the error in our diagnostics timeline and generate a crash dump.
// ---------------------------------------------------------------------------

namespace seed::profiling {

static bool g_sanitizerHooksInstalled = false;

void installSanitizerHooks() {
    if (g_sanitizerHooksInstalled) return;
    g_sanitizerHooksInstalled = true;

    // The actual hook installation is done via compile-time flags:
    // -fsanitize=address,undefined -fno-omit-frame-pointer
    // The sanitizer runtime calls our __asan_on_error automatically.
}

} // namespace seed::profiling

// ---------------------------------------------------------------------------
// AddressSanitizer Hooks
// ---------------------------------------------------------------------------
extern "C" {

void __asan_on_error() {
    using namespace seed::diagnostics;

    SEED_DIAG_EVENT(EventType::AssertionFail, seed::ecs::INVALID_ENTITY, 0, 0, 0,
                    "AddressSanitizer detected error", "<asan>", 0);

    auto& diag = DiagnosticsManager::instance();
    diag.health().setScore(HealthScore::Module::Memory, 0);

    fmt::print(stderr, "\n[ASAN ERROR] AddressSanitizer detected memory corruption\n");
    fmt::print(stderr, "Diagnostic timeline captured. Check artifacts for full dump.\n");

    // Attempt to write emergency log
    diag.writeToFile("asan_crash.log");

    // Let ASan print its own stack trace, then abort
}

void __asan_report_error(const char* file, int line, const char* func, const char* msg) {
    using namespace seed::diagnostics;

    SEED_DIAG_EVENT(EventType::AssertionFail, seed::ecs::INVALID_ENTITY, 0, 0, 0,
                    msg, file, line);

    fmt::print(stderr, "[ASAN] {}:{} in {}: {}\n", file, line, func, msg);
}

// ---------------------------------------------------------------------------
// UndefinedBehaviorSanitizer Hooks
// ---------------------------------------------------------------------------

void __ubsan_handle_type_mismatch() {
    using namespace seed::diagnostics;

    SEED_DIAG_EVENT(EventType::AssertionFail, seed::ecs::INVALID_ENTITY, 0, 0, 0,
                    "UBSan: type mismatch", "<ubsan>", 0);

    auto& diag = DiagnosticsManager::instance();
    diag.health().setScore(HealthScore::Module::ECS, 20);

    fmt::print(stderr, "[UBSAN] Type mismatch detected\n");
}

void __ubsan_handle_alignment_assumption() {
    using namespace seed::diagnostics;

    SEED_DIAG_EVENT(EventType::AssertionFail, seed::ecs::INVALID_ENTITY, 0, 0, 0,
                    "UBSan: alignment assumption violated", "<ubsan>", 0);

    fmt::print(stderr, "[UBSAN] Alignment assumption violated\n");
}

} // extern "C"
