/**
 * @file logger.cpp
 * @brief Logger implementation
 */

#include <phoenix/core/logger.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace phoenix {

static std::shared_ptr<spdlog::logger> s_logger;

void initLogging(const std::string& appName, spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(level);
    
    s_logger = std::make_shared<spdlog::logger>(appName, console_sink);
    s_logger->set_level(level);
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    
    spdlog::set_default_logger(s_logger);
}

std::shared_ptr<spdlog::logger> getLogger() {
    if (!s_logger) {
        initLogging("phoenix");
    }
    return s_logger;
}

void setLogLevel(spdlog::level::level_enum level) {
    if (s_logger) {
        s_logger->set_level(level);
    }
}

} // namespace phoenix
