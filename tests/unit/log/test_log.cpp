#include <doctest/doctest.h>
#include "core/log/log.h"
#include <filesystem>
#include <fstream>

using namespace seed::log;

TEST_CASE("LogSystem_Initialize") {
    LogSystem::instance().initialize("test_seed.log", 1024 * 1024, 3);
    auto logger = LogSystem::instance().logger();
    CHECK(logger != nullptr);
}

TEST_CASE("LogSystem_LogLevels") {
    LogSystem::instance().initialize("test_seed_levels.log", 1024 * 1024, 3);
    auto logger = LogSystem::instance().logger();
    CHECK_NOTHROW(logger->info("Test info message"));
    CHECK_NOTHROW(logger->warn("Test warn message"));
    CHECK_NOTHROW(logger->error("Test error message"));
}

TEST_CASE("LogSystem_FileRotation") {
    const char* testFile = "test_rotation.log";
    std::filesystem::remove(testFile);

    LogSystem::instance().initialize(testFile, 1024, 3);
    auto logger = LogSystem::instance().logger();

    for (int i = 0; i < 200; ++i) {
        logger->info("This is a long log message that should eventually trigger file rotation {}", i);
    }

    CHECK(std::filesystem::exists(testFile));
}
