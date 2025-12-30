#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <thread>

namespace StreamKit {

/**
 * @brief 错误严重级别
 */
enum class ErrorSeverity {
    INFO,       // 信息性错误，可继续
    WARNING,    // 警告，可能影响性能但不影响功能
    RECOVERABLE,// 可恢复错误，需要重试或降级
    FATAL       // 致命错误，无法恢复
};

/**
 * @brief 错误类型分类
 */
enum class ErrorType {
    DECODE_ERROR,          // 解码错误
    HARDWARE_FAILURE,      // 硬件加速失败
    MEMORY_ERROR,          // 内存错误
    IO_ERROR,              // I/O 错误
    NETWORK_ERROR,         // 网络错误
    INVALID_STATE,         // 无效状态
    TIMEOUT,               // 超时
    UNKNOWN                // 未知错误
};

/**
 * @brief 错误信息结构
 */
struct ErrorInfo {
    ErrorType type;
    ErrorSeverity severity;
    std::string message;
    std::string context;      // 额外的上下文信息
    int errorCode;            // 系统错误代码
    std::chrono::system_clock::time_point timestamp;

    ErrorInfo()
        : type(ErrorType::UNKNOWN)
        , severity(ErrorSeverity::INFO)
        , errorCode(0)
        , timestamp(std::chrono::system_clock::now())
    {}
};

/**
 * @brief 重试策略配置
 */
struct RetryPolicy {
    int max_attempts;              // 最大重试次数
    std::chrono::milliseconds base_delay;  // 基础延迟
    std::chrono::milliseconds max_delay;   // 最大延迟
    double backoff_multiplier;     // 退避倍数
    bool enable_jitter;            // 是否启用随机抖动

    RetryPolicy()
        : max_attempts(3)
        , base_delay(100)
        , max_delay(5000)
        , backoff_multiplier(2.0)
        , enable_jitter(true)
    {}
};

/**
 * @brief 恢复动作结果
 */
enum class RecoveryResult {
    SUCCESS,           // 恢复成功
    FAILED,            // 恢复失败
    RETRY_LATER,       // 稍后重试
    FALLBACK_TRIGGERED // 触发降级
};

/**
 * @brief 错误恢复动作函数类型
 * @param error 错误信息
 * @param attempt 当前尝试次数
 * @return 恢复结果
 */
using RecoveryAction = std::function<RecoveryResult(const ErrorInfo& error, int attempt)>;

/**
 * @brief 错误统计信息
 */
struct ErrorStats {
    int total_errors;
    int recovered_errors;
    int fatal_errors;
    std::map<ErrorType, int> errors_by_type;

    ErrorStats()
        : total_errors(0)
        , recovered_errors(0)
        , fatal_errors(0)
    {}

    double getRecoveryRate() const {
        return total_errors > 0 ? static_cast<double>(recovered_errors) / total_errors : 0.0;
    }
};

/**
 * @brief 错误恢复管理器
 *
 * 提供统一的错误处理、重试和恢复机制
 */
class ErrorRecoveryManager {
public:
    ErrorRecoveryManager();
    ~ErrorRecoveryManager() = default;

    /**
     * @brief 设置重试策略
     * @param policy 重试策略
     */
    void setRetryPolicy(const RetryPolicy& policy);

    /**
     * @brief 注册特定错误类型的恢复动作
     * @param type 错误类型
     * @param action 恢复动作
     */
    void registerRecoveryAction(ErrorType type, RecoveryAction action);

    /**
     * @brief 处理错误，尝试恢复
     * @param error 错误信息
     * @return true 如果恢复成功或错误可忽略
     */
    bool handleError(const ErrorInfo& error);

    /**
     * @brief 使用默认恢复动作处理错误
     * @param type 错误类型
     * @param message 错误消息
     * @param errorCode 错误代码
     * @return true 如果恢复成功
     */
    bool handleError(ErrorType type, const std::string& message, int errorCode = 0);

    /**
     * @brief 获取错误统计
     * @return 错误统计信息
     */
    ErrorStats getStats() const;

    /**
     * @brief 重置错误统计
     */
    void resetStats();

    /**
     * @brief 获取最近错误
     * @return 最近的错误信息
     */
    std::vector<ErrorInfo> getRecentErrors(int count = 10) const;

    /**
     * @brief 清除错误历史
     */
    void clearErrorHistory();

    /**
     * @brief 设置全局降级处理器
     * @param handler 降级处理函数（当所有恢复尝试失败时调用）
     */
    void setFallbackHandler(std::function<void(const ErrorInfo&)> handler);

    /**
     * @brief 获取当前重试策略
     * @return 重试策略的常量引用
     */
    const RetryPolicy& getRetryPolicy() const { return retry_policy_; }

private:
    /**
     * @brief 计算重试延迟
     * @param attempt 当前尝试次数
     * @return 延迟时长
     */
    std::chrono::milliseconds calculateDelay(int attempt) const;

    /**
     * @brief 记录错误
     * @param error 错误信息
     * @param recovered 是否成功恢复
     */
    void recordError(const ErrorInfo& error, bool recovered);

    mutable std::mutex mutex_;
    RetryPolicy retry_policy_;
    std::map<ErrorType, RecoveryAction> recovery_actions_;
    ErrorStats stats_;
    std::vector<ErrorInfo> error_history_;
    size_t max_history_size_;
    std::function<void(const ErrorInfo&)> fallback_handler_;
};

/**
 * @brief 自动错误恢复辅助类
 *
 * 用于在函数调用失败时自动重试
 */
template<typename Func, typename... Args>
auto withRetry(ErrorRecoveryManager& manager,
               ErrorType error_type,
               const std::string& operation_name,
               Func&& func,
               Args&&... args) -> decltype(func(args...)) {
    using ReturnType = decltype(func(args...));
    const RetryPolicy& policy = manager.getRetryPolicy();

    for (int attempt = 0; attempt <= policy.max_attempts; ++attempt) {
        try {
            if (attempt > 0) {
                auto delay = manager.calculateDelay(attempt);
                std::this_thread::sleep_for(delay);
            }

            return func(std::forward<Args>(args)...);
        } catch (const std::exception& e) {
            ErrorInfo error;
            error.type = error_type;
            error.message = operation_name + " failed: " + e.what();
            error.context = "Attempt " + std::to_string(attempt + 1);

            if (attempt == policy.max_attempts) {
                error.severity = ErrorSeverity::FATAL;
                manager.handleError(error);
                throw;
            } else {
                error.severity = ErrorSeverity::RECOVERABLE;
                manager.handleError(error);
            }
        }
    }

    // 不应该到达这里，但为了编译完整性
    throw std::runtime_error("Retry loop terminated unexpectedly");
}

/**
 * @brief 将错误类型转换为字符串
 * @param type 错误类型
 * @return 字符串表示
 */
inline std::string errorTypeToString(ErrorType type) {
    switch (type) {
        case ErrorType::DECODE_ERROR: return "DecodeError";
        case ErrorType::HARDWARE_FAILURE: return "HardwareFailure";
        case ErrorType::MEMORY_ERROR: return "MemoryError";
        case ErrorType::IO_ERROR: return "IOError";
        case ErrorType::NETWORK_ERROR: return "NetworkError";
        case ErrorType::INVALID_STATE: return "InvalidState";
        case ErrorType::TIMEOUT: return "Timeout";
        default: return "Unknown";
    }
}

/**
 * @brief 将错误严重级别转换为字符串
 * @param severity 错误严重级别
 * @return 字符串表示
 */
inline std::string errorSeverityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::INFO: return "Info";
        case ErrorSeverity::WARNING: return "Warning";
        case ErrorSeverity::RECOVERABLE: return "Recoverable";
        case ErrorSeverity::FATAL: return "Fatal";
        default: return "Unknown";
    }
}

} // namespace StreamKit
