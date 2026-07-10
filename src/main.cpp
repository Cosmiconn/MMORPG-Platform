#include <cstdio>
#include <cstdlib>
#include <string>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

// Tracy profiler integration (optional – no-op if Tracy not found)
#if __has_include(<tracy/Tracy.hpp>)
#  include <tracy/Tracy.hpp>
#  define SEED_ZONE(name) ZoneScopedN(name)
#else
   // Use sizeof to consume the string literal without generating code;
   // avoids MSVC C4702 "unreachable code" when (void)0 is used as a statement.
#  define SEED_ZONE(name) ((void)sizeof(name))
#endif

// ---------------------------------------------------------------------------
// Smoke-test helpers
// ---------------------------------------------------------------------------
static bool test_spdlog() {
    SEED_ZONE("test_spdlog");
    try {
        spdlog::info("spdlog smoke test: {}", 42);
        spdlog::warn("spdlog warning channel OK");
        return true;
    } catch (...) {
        return false;
    }
}

static bool test_fmt() {
    SEED_ZONE("test_fmt");
    std::string s = fmt::format("fmt test: pi = {:.5f}", 3.14159);
    return s.find("3.14159") != std::string::npos;
}

static bool test_json() {
    SEED_ZONE("test_json");
    nlohmann::json j;
    j["engine"]   = "TheSeed";
    j["version"]  = "0.1.0";
    j["phase"]    = "P0-M1";
    j["features"] = nlohmann::json::array({"build-system", "vcpkg", "ci-cd"});
    return j.contains("engine") && j["features"].size() == 3;
}

static bool test_cpp20() {
    SEED_ZONE("test_cpp20");
    // if constexpr with explicit else avoids MSVC C4702 on generic lambda
    auto is_numeric = [](auto x) -> bool {
        if constexpr (std::is_arithmetic_v<decltype(x)>) {
            return true;
        } else {
            return false;
        }
    };
    return is_numeric(42) && is_numeric(3.14f) && !is_numeric(std::string("hello"));
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    SEED_ZONE("main");
    (void)argc;
    (void)argv;

    fmt::print("\n=== TheSeed Smoke Test ===\n");
    fmt::print("C++ Standard: {}\n", __cplusplus);
    fmt::print("Build: P0-M1 (Build-System & CI/CD)\n\n");

    bool ok = true;

    ok &= test_cpp20();
    fmt::print("[{}] C++20 concepts\n", ok ? "PASS" : "FAIL");

    ok &= test_spdlog();
    fmt::print("[{}] spdlog logging\n", ok ? "PASS" : "FAIL");

    ok &= test_fmt();
    fmt::print("[{}] fmt formatting\n", ok ? "PASS" : "FAIL");

    ok &= test_json();
    fmt::print("[{}] nlohmann/json\n", ok ? "PASS" : "FAIL");

    fmt::print("\n==========================\n");
    if (ok) {
        spdlog::info("All smoke tests passed. Ready for P0-M2.");
        return EXIT_SUCCESS;
    } else {
        spdlog::error("Smoke test FAILED!");
        return EXIT_FAILURE;
    }
}
