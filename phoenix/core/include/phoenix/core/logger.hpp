/**
 * @file logger.hpp
 * @brief Logging utilities wrapping spdlog
 * 
 * Provides a thin wrapper around spdlog with convenient macros.
 */

#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace phoenix {

/// Initialize logging system (call once at startup)
void initLogging(const std::string& appName, spdlog::level::level_enum level = spdlog::level::info);

/// Get default logger
std::shared_ptr<spdlog::logger> getLogger();

/// Set log level at runtime
void setLogLevel(spdlog::level::level_enum level);

} // namespace phoenix

// Convenience macros for logging (global scope, simple names)
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(phoenix::getLogger(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(phoenix::getLogger(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(phoenix::getLogger(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(phoenix::getLogger(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(phoenix::getLogger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(phoenix::getLogger(), __VA_ARGS__)
