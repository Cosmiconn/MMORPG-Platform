#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace seed::log {

class LogSystem {
public:
    static LogSystem& instance();
    void initialize(const char* logFile = "logs/seed.log", size_t maxFileSize = 100 * 1024 * 1024, size_t maxFiles = 5);
    std::shared_ptr<spdlog::logger> logger();

private:
    LogSystem() = default;
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace seed::log

#define SEED_LOG_TRACE(...)  SPDLOG_LOGGER_TRACE(seed::log::LogSystem::instance().logger(), __VA_ARGS__)
#define SEED_LOG_DEBUG(...)  SPDLOG_LOGGER_DEBUG(seed::log::LogSystem::instance().logger(), __VA_ARGS__)
#define SEED_LOG_INFO(...)   SPDLOG_LOGGER_INFO(seed::log::LogSystem::instance().logger(), __VA_ARGS__)
#define SEED_LOG_WARN(...)   SPDLOG_LOGGER_WARN(seed::log::LogSystem::instance().logger(), __VA_ARGS__)
#define SEED_LOG_ERROR(...)  SPDLOG_LOGGER_ERROR(seed::log::LogSystem::instance().logger(), __VA_ARGS__)
#define SEED_LOG_FATAL(...)  SPDLOG_LOGGER_CRITICAL(seed::log::LogSystem::instance().logger(), __VA_ARGS__)
