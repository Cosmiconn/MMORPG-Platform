#include <fmt/format.h>
#include "core/diagnostics/diagnostics_manager.h"

int main() {
    fmt::print("TheSeed Engine v{}\n", "0.1.0");

    auto& diag = seed::diagnostics::DiagnosticsManager::instance();
    diag.initialize();

    fmt::print("Diagnostics initialized. Health: {}\n", 
        diag.isHealthy() ? "OK" : "FAIL");

    return 0;
}
