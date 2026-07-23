#include "core/log/log.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace seed::log {

LogSystem& LogSystem::instance() {
    static LogSystem sys;
    return sys;
}

void LogSystem::initialize(const char* logFile, size_t maxFileSize, size_t maxFiles) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, maxFileSize, maxFiles);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    m_logger = std::make_shared<spdlog::logger>("seed", sinks.begin(), sinks.end());
    m_logger->set_level(spdlog::level::debug);
    m_logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(m_logger);
}

std::shared_ptr<spdlog::logger> LogSystem::logger() {
    if (!m_logger) initialize();
    return m_logger;
}

} // namespace seed::log
