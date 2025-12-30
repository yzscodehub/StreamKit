#include "common/error_recovery.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <random>
#include <thread>
#include <sstream>

namespace StreamKit {

ErrorRecoveryManager::ErrorRecoveryManager()
    : max_history_size_(100)
{
    // 设置默认重试策略
    retry_policy_ = RetryPolicy();

    // 注册默认的恢复动作
    registerRecoveryAction(ErrorType::HARDWARE_FAILURE,
        [](const ErrorInfo& error, int attempt) -> RecoveryResult {
            LOG_WARN("Hardware failure detected, attempting recovery (attempt {})", attempt);

            if (attempt < 2) {
                // 前几次尝试重试
                return RecoveryResult::RETRY_LATER;
            } else {
                // 超过重试次数，触发降级到软件解码
                LOG_WARN("Hardware recovery failed, triggering fallback to software");
                return RecoveryResult::FALLBACK_TRIGGERED;
            }
        });

    registerRecoveryAction(ErrorType::DECODE_ERROR,
        [](const ErrorInfo& error, int attempt) -> RecoveryResult {
            if (error.severity == ErrorSeverity::FATAL) {
                return RecoveryResult::FAILED;
            }

            // 解码错误可以重试几次
            if (attempt < 3) {
                return RecoveryResult::RETRY_LATER;
            }

            return RecoveryResult::FAILED;
        });

    registerRecoveryAction(ErrorType::MEMORY_ERROR,
        [](const ErrorInfo& error, int attempt) -> RecoveryResult {
            LOG_ERROR("Memory error detected: {}", error.message);

            // 内存错误通常不应该重试
            if (attempt > 0) {
                return RecoveryResult::FAILED;
            }

            // 可以尝试清理缓存
            return RecoveryResult::RETRY_LATER;
        });

    registerRecoveryAction(ErrorType::IO_ERROR,
        [](const ErrorInfo& error, int attempt) -> RecoveryResult {
            LOG_WARN("I/O error detected: {}", error.message);

            // I/O 错误可以多次重试
            if (attempt < 5) {
                return RecoveryResult::RETRY_LATER;
            }

            return RecoveryResult::FAILED;
        });
}

void ErrorRecoveryManager::setRetryPolicy(const RetryPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    retry_policy_ = policy;
    LOG_INFO("Retry policy updated: max_attempts={}, base_delay={}ms",
             policy.max_attempts, policy.base_delay.count());
}

void ErrorRecoveryManager::registerRecoveryAction(ErrorType type, RecoveryAction action) {
    std::lock_guard<std::mutex> lock(mutex_);
    recovery_actions_[type] = std::move(action);
    LOG_DEBUG("Recovery action registered for error type: {}", static_cast<int>(type));
}

bool ErrorRecoveryManager::handleError(const ErrorInfo& error) {
    // 记录错误到历史（需要锁）
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (error_history_.size() >= max_history_size_) {
            error_history_.erase(error_history_.begin());
        }
        error_history_.push_back(error);

        // 更新统计
        stats_.total_errors++;
        stats_.errors_by_type[error.type]++;

        // 根据严重级别处理
        if (error.severity == ErrorSeverity::FATAL) {
            stats_.fatal_errors++;
            LOG_ERROR("Fatal error encountered: {} - {}", errorTypeToString(error.type), error.message);

            // 调用降级处理器（在锁外调用）
            auto handler = fallback_handler_;
            lock.unlock(); // 手动释放锁

            if (handler) {
                try {
                    handler(error);
                } catch (const std::exception& e) {
                    LOG_ERROR("Fallback handler threw exception: {}", e.what());
                }
            }

            recordError(error, false);
            return false;
        }
    }

    // 查找并执行恢复动作
    auto action_it = recovery_actions_.find(error.type);
    if (action_it != recovery_actions_.end()) {
        const auto& action = action_it->second;

        for (int attempt = 0; attempt <= retry_policy_.max_attempts; ++attempt) {
            RecoveryResult result = action(error, attempt);

            if (result == RecoveryResult::SUCCESS) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.recovered_errors++;
                }
                recordError(error, true);
                LOG_INFO("Error recovered successfully after {} attempts", attempt + 1);
                return true;
            }

            if (result == RecoveryResult::FALLBACK_TRIGGERED) {
                LOG_WARN("Fallback triggered for error: {}", error.message);
                auto handler = fallback_handler_;
                if (handler) {
                    try {
                        handler(error);
                        std::lock_guard<std::mutex> lock(mutex_);
                        stats_.recovered_errors++;
                        recordError(error, true);
                        return true;
                    } catch (const std::exception& e) {
                        LOG_ERROR("Fallback handler failed: {}", e.what());
                    }
                }
                break;
            }

            if (result == RecoveryResult::FAILED) {
                break;
            }

            if (result == RecoveryResult::RETRY_LATER) {
                if (attempt < retry_policy_.max_attempts) {
                    auto delay = calculateDelay(attempt + 1);
                    LOG_DEBUG("Retrying after {}ms...", delay.count());
                    std::this_thread::sleep_for(delay);
                }
            }
        }
    } else {
        LOG_WARN("No recovery action registered for error type: {}", static_cast<int>(error.type));
    }

    recordError(error, false);
    return false;
}

bool ErrorRecoveryManager::handleError(ErrorType type, const std::string& message, int errorCode) {
    ErrorInfo error;
    error.type = type;
    error.message = message;
    error.errorCode = errorCode;
    error.timestamp = std::chrono::system_clock::now();

    // 根据错误类型推断严重级别
    switch (type) {
        case ErrorType::MEMORY_ERROR:
            error.severity = ErrorSeverity::FATAL;
            break;
        case ErrorType::HARDWARE_FAILURE:
        case ErrorType::DECODE_ERROR:
            error.severity = ErrorSeverity::RECOVERABLE;
            break;
        default:
            error.severity = ErrorSeverity::WARNING;
            break;
    }

    return handleError(error);
}

ErrorStats ErrorRecoveryManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void ErrorRecoveryManager::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = ErrorStats();
    LOG_INFO("Error statistics reset");
}

std::vector<ErrorInfo> ErrorRecoveryManager::getRecentErrors(int count) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ErrorInfo> recent;
    auto start_it = error_history_.size() > static_cast<size_t>(count)
        ? error_history_.end() - count
        : error_history_.begin();

    recent.insert(recent.end(), start_it, error_history_.end());
    return recent;
}

void ErrorRecoveryManager::clearErrorHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    error_history_.clear();
    LOG_INFO("Error history cleared");
}

void ErrorRecoveryManager::setFallbackHandler(std::function<void(const ErrorInfo&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    fallback_handler_ = std::move(handler);
    LOG_INFO("Fallback handler registered");
}

std::chrono::milliseconds ErrorRecoveryManager::calculateDelay(int attempt) const {
    // 指数退避算法
    auto delay = retry_policy_.base_delay *
                 static_cast<int>(std::pow(retry_policy_.backoff_multiplier, attempt - 1));

    // 限制最大延迟
    delay = std::min(delay, retry_policy_.max_delay);

    // 添加随机抖动
    if (retry_policy_.enable_jitter) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(-delay.count() / 4, delay.count() / 4);
        delay = std::chrono::milliseconds(delay.count() + dis(gen));
        delay = std::max(std::chrono::milliseconds(0), delay);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(delay);
}

void ErrorRecoveryManager::recordError(const ErrorInfo& error, bool recovered) {
    // 记录已在上层方法中完成
    if (recovered) {
        LOG_INFO("Error recovered: {} - {}", errorTypeToString(error.type), error.message);
    } else {
        LOG_ERROR("Error not recovered: {} - {}", errorTypeToString(error.type), error.message);
    }
}

} // namespace StreamKit
