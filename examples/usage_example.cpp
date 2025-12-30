/**
 * @file usage_example.cpp
 * @brief 演示如何使用 StreamKit 的新功能：Config、ErrorRecovery、RAII Wrappers
 */

#include "common/config.hpp"
#include "common/error_recovery.hpp"
#include "common/raii_wrappers.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace StreamKit;

void exampleConfigUsage() {
    std::cout << "\n=== Config 使用示例 ===\n";

    // 1. 加载默认配置
    Config& config = Config::getInstance();
    config.loadDefaults();

    // 2. 读取配置
    int width = config.get<int>("video.width", 1920);
    int height = config.get<int>("video.height", 1080);
    std::string codec = config.get<std::string>("video.codec", "h264");

    std::cout << "视频配置: " << width << "x" << height
              << ", 编解码器: " << codec << "\n";

    // 3. 修改配置
    config.set("video.width", 2560);
    config.set("video.fps", 60);

    // 4. 检查键是否存在
    if (config.has("video.bitrate")) {
        int bitrate = config.get<int>("video.bitrate");
        std::cout << "比特率: " << bitrate << " bps\n";
    }

    // 5. 获取所有键
    auto keys = config.getKeys("video");
    std::cout << "视频配置键:\n";
    for (const auto& key : keys) {
        std::cout << "  - " << key << "\n";
    }

    // 6. 设置配置验证器
    config.setValidator([](const std::string& key, const nlohmann::json& value) {
        if (key == "video.fps") {
            int fps = value.get<int>();
            return fps > 0 && fps <= 120;
        }
        return true;
    });

    // 验证失败
    if (!config.set("video.fps", 150, true)) {
        std::cout << "配置验证失败: FPS 不能超过 120\n";
    }

    // 7. 添加变更监听器
    size_t listenerId = config.addChangeListener(
        [](const std::string& key, const nlohmann::json& oldVal, const nlohmann::json& newVal) {
            std::cout << "配置变更: " << key
                      << " = " << oldVal << " -> " << newVal << "\n";
        }
    );

    config.set("video.bitrate", 8000000);
    config.removeChangeListener(listenerId);

    // 8. 保存到文件
    try {
        config.saveToFile("config_example.json", true);
        std::cout << "配置已保存到 config_example.json\n";
    } catch (const std::exception& e) {
        std::cerr << "保存配置失败: " << e.what() << "\n";
    }
}

void exampleErrorRecoveryUsage() {
    std::cout << "\n=== 错误恢复使用示例 ===\n";

    ErrorRecoveryManager recoveryManager;

    // 1. 设置自定义重试策略
    RetryPolicy policy;
    policy.max_attempts = 5;
    policy.base_delay = std::chrono::milliseconds(200);
    policy.backoff_multiplier = 1.5;
    recoveryManager.setRetryPolicy(policy);

    // 2. 设置降级处理器
    recoveryManager.setFallbackHandler([](const ErrorInfo& error) {
        std::cout << "[降级] 触发降级处理: " << error.message << "\n";
        std::cout << "[降级] 切换到软件解码模式...\n";
    });

    // 3. 注册自定义恢复动作
    recoveryManager.registerRecoveryAction(ErrorType::DECODE_ERROR,
        [](const ErrorInfo& error, int attempt) -> RecoveryResult {
            std::cout << "[重试] 解码错误恢复，尝试 " << attempt << "\n";

            if (attempt >= 2) {
                std::cout << "[重试] 超过最大重试次数，放弃\n";
                return RecoveryResult::FAILED;
            }

            return RecoveryResult::RETRY_LATER;
        });

    // 4. 模拟错误处理
    ErrorInfo error;
    error.type = ErrorType::DECODE_ERROR;
    error.severity = ErrorSeverity::RECOVERABLE;
    error.message = "帧解码失败";
    error.errorCode = -1;

    bool recovered = recoveryManager.handleError(error);
    std::cout << "恢复结果: " << (recovered ? "成功" : "失败") << "\n";

    // 5. 使用便捷方法处理错误
    recoveryManager.handleError(ErrorType::IO_ERROR, "文件读取失败", 5);

    // 6. 查看错误统计
    ErrorStats stats = recoveryManager.getStats();
    std::cout << "\n错误统计:\n";
    std::cout << "  总错误数: " << stats.total_errors << "\n";
    std::cout << "  已恢复: " << stats.recovered_errors << "\n";
    std::cout << "  恢复率: " << (stats.getRecoveryRate() * 100.0) << "%\n";

    // 7. 查看最近的错误
    auto recentErrors = recoveryManager.getRecentErrors(5);
    std::cout << "\n最近的错误:\n";
    for (const auto& err : recentErrors) {
        std::cout << "  [" << errorTypeToString(err.type) << "] "
                  << err.message << "\n";
    }
}

void exampleRAIIWrappersUsage() {
    std::cout << "\n=== RAII 包装器使用示例 ===\n";

    // 1. AVFrame 的安全使用
    {
        AVFramePtr frame = makeAVFrame();
        if (frame) {
            frame->width = 1920;
            frame->height = 1080;
            frame->format = AV_PIX_FMT_YUV420P;

            // 使用 frame...
            std::cout << "AVFrame 创建成功: " << frame->width << "x" << frame->height << "\n";
        }
        // frame 自动释放
    }

    // 2. AVPacket 的安全使用
    {
        AVPacketPtr packet = makeAVPacket();
        if (packet) {
            // 使用 packet...
            std::cout << "AVPacket 创建成功\n";
        }
        // packet 自动释放
    }

    // 3. SwsContext 的安全使用
    {
        SwsContextPtr swsCtx = makeSwsContext(
            1920, 1080, AV_PIX_FMT_YUV420P,
            1280, 720, AV_PIX_FMT_RGB24
        );

        if (swsCtx) {
            std::cout << "SwsContext 创建成功\n";
        }
        // swsCtx 自动释放
    }

    // 4. ScopeGuard 用于任意资源的清理
    {
        FILE* file = fopen("test.txt", "w");
        auto guard = makeScopeGuard([&file]() {
            if (file) {
                fclose(file);
                std::cout << "文件已自动关闭\n";
            }
        });

        if (file) {
            fprintf(file, "Hello, RAII!\n");
        }
        // guard 在作用域结束时自动关闭文件
    }

    // 5. CUDA 资源管理
    {
        CudaDevicePtr deviceMem = makeCudaDevicePtr(1024 * 1024);
        if (deviceMem) {
            std::cout << "CUDA 设备内存分配成功: 1MB\n";
        }
        // deviceMem 自动释放

        CudaEventPtr event = makeCudaEvent(cudaEventDisableTiming);
        if (event) {
            std::cout << "CUDA 事件创建成功\n";
        }
        // event 自动释放

        CudaStreamPtr stream = makeCudaStream(cudaStreamNonBlocking);
        if (stream) {
            std::cout << "CUDA 流创建成功\n";
        }
        // stream 自动释放
    }

    std::cout << "\n所有资源已自动清理，无泄漏！\n";
}

void exampleIntegrationUsage() {
    std::cout << "\n=== 集成使用示例 ===\n";

    // 结合使用 Config、ErrorRecovery 和 RAII

    ErrorRecoveryManager recoveryManager;

    // 使用 RAII 创建 AVFormatContext
    AVFormatContextPtr formatCtx = makeAVFormatContext("test.mp4");
    if (!formatCtx) {
        ErrorInfo error;
        error.type = ErrorType::IO_ERROR;
        error.message = "无法打开视频文件";
        recoveryManager.handleError(error);
        return;
    }

    std::cout << "视频文件打开成功\n";

    // 使用 Config 配置重试策略
    Config& config = Config::getInstance();
    RetryPolicy policy;
    policy.max_attempts = config.get<int>("decoder.maxRetries", 3);
    policy.base_delay = std::chrono::milliseconds(
        config.get<int>("decoder.retryDelayMs", 100)
    );
    recoveryManager.setRetryPolicy(policy);

    std::cout << "配置已应用到错误恢复管理器\n";
}

int main() {
    std::cout << "StreamKit 新功能使用示例\n";
    std::cout << "========================\n";

    try {
        exampleConfigUsage();
        exampleErrorRecoveryUsage();
        exampleRAIIWrappersUsage();
        exampleIntegrationUsage();

        std::cout << "\n所有示例执行完成！\n";
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
