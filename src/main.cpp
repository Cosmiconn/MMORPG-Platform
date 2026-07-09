#include <cstdio>
#include <doctest/doctest.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
int main(int argc, char** argv) {
  spdlog::info("TheSeed build system initialized");
  nlohmann::json config;
  config["engine"] = "TheSeed";
  config["version"] = "0.1.0";
  config["phase"] = "P0-M1";
  fmt::print("\n=== TheSeed Smoke Test ===\n");
  fmt::print("Config: {}\n", config.dump(2));
  fmt::print("C++ Standard: {}\n", __cplusplus);
  fmt::print("==========================\n\n");
  if (argc > 1 && std::string(argv[1]) == "--test") {
    doctest::Context ctx;
    ctx.setOption("no-break", true);
    return ctx.run();
  }
  return 0;
}
