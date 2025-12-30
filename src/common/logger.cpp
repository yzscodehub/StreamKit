#include "common/logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>

namespace StreamKit {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initialize(const std::string& logFile) {
    getInstance().initializeInstance(logFile);
}

void Logger::initializeInstance(const std::string& logFile) {
    try {
        // 创建控制台输出
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        // 创建文件输出
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFile, 1024 * 1024 * 10, 3);  // 10MB, 3 files
        file_sink->set_level(spdlog::level::debug);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        
        // 创建logger
        logger_ = std::make_shared<spdlog::logger>("streamkit", 
            spdlog::sinks_init_list{console_sink, file_sink});
        logger_->set_level(spdlog::level::debug);
        
        // 设置为全局默认logger
        spdlog::set_default_logger(logger_);
        
        LOG_INFO("Logger initialized successfully");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger> Logger::getLogger() const {
    return logger_;
}

} // namespace StreamKit 