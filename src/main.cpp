#include <cstdio>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
        
    (void)argc;  // suppress unused parameter warning
    (void)argv;  // suppress unused parameter warning
    
    spdlog::info("TheSeed build system initialized");
    
    nlohmann::json config;
    config["engine"] = "TheSeed";
    config["version"] = "0.1.0";
    config["phase"] = "P0-M1";
    
    fmt::print("\n=== TheSeed Smoke Test ===\n");
    fmt::print("Config: {}\n", config.dump(2));
    fmt::print("C++ Standard: {}\n", __cplusplus);
    fmt::print("==========================\n\n");
    
    return 0;
}
