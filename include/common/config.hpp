#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

namespace StreamKit {

/**
 * @brief 配置验证回调类型
 * @param key 配置键
 * @param value 配置值
 * @return true 如果验证通过，false 否则
 */
using ConfigValidator = std::function<bool(const std::string& key, const nlohmann::json& value)>;

/**
 * @brief 配置变更监听器类型
 * @param key 被修改的配置键
 * @param oldValue 旧值
 * @param newValue 新值
 */
using ConfigChangeListener = std::function<void(const std::string& key, const nlohmann::json& oldValue, const nlohmann::json& newValue)>;

class Config {
public:
    static Config& getInstance();

    /**
     * @brief 从文件加载配置
     * @param configFile 配置文件路径
     * @param merge 是否与现有配置合并 (true) 或替换 (false)
     * @throw std::runtime_error 如果文件无法读取或解析失败
     */
    void loadFromFile(const std::string& configFile, bool merge = true);

    /**
     * @brief 从 JSON 对象加载配置
     * @param json JSON 配置对象
     * @param merge 是否与现有配置合并
     */
    void loadFromJson(const nlohmann::json& json, bool merge = true);

    /**
     * @brief 获取配置值
     * @tparam T 值类型
     * @param key 配置键 (支持点号分隔的嵌套访问，如 "video.width")
     * @param defaultValue 默认值
     * @return 配置值
     */
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const;

    /**
     * @brief 设置配置值
     * @tparam T 值类型
     * @param key 配置键 (支持点号分隔的嵌套访问)
     * @param value 配置值
     * @param validate 是否进行验证
     * @return true 如果设置成功，false 如果验证失败
     */
    template<typename T>
    bool set(const std::string& key, const T& value, bool validate = true);

    /**
     * @brief 检查配置键是否存在
     * @param key 配置键
     * @return true 如果存在
     */
    bool has(const std::string& key) const;

    /**
     * @brief 删除配置项
     * @param key 配置键
     * @return true 如果删除成功
     */
    bool remove(const std::string& key);

    /**
     * @brief 保存配置到文件
     * @param configFile 配置文件路径
     * @param pretty 是否格式化输出 (带缩进)
     */
    void saveToFile(const std::string& configFile, bool pretty = true) const;

    /**
     * @brief 获取完整的 JSON 配置对象
     * @return JSON 对象的副本
     */
    nlohmann::json toJson() const;

    /**
     * @brief 设置配置验证器
     * @param validator 验证函数
     */
    void setValidator(ConfigValidator validator);

    /**
     * @brief 添加配置变更监听器
     * @param listener 监听器函数
     * @return 监听器 ID (用于移除)
     */
    size_t addChangeListener(ConfigChangeListener listener);

    /**
     * @brief 移除配置变更监听器
     * @param listenerId 监听器 ID
     */
    void removeChangeListener(size_t listenerId);

    /**
     * @brief 获取所有配置键
     * @param prefix 键前缀过滤 (可选)
     * @return 配置键列表
     */
    std::vector<std::string> getKeys(const std::string& prefix = "") const;

    /**
     * @brief 清空所有配置
     */
    void clear();

    /**
     * @brief 加载默认配置
     */
    void loadDefaults();

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /**
     * @brief 通过点号分隔的键获取 JSON 引用
     * @param key 配置键
     * @param createMissing 是否创建缺失的中间对象
     * @return JSON 引用
     */
    nlohmann::json& getByNestedKey(const std::string& key, bool createMissing = false);

    /**
     * @brief 通知监听器配置已变更
     */
    void notifyListeners(const std::string& key, const nlohmann::json& oldValue, const nlohmann::json& newValue);

    nlohmann::json config_;
    mutable std::mutex mutex_;
    ConfigValidator validator_;
    std::map<size_t, ConfigChangeListener> listeners_;
    size_t next_listener_id_ = 1;
};

// 默认配置
struct DefaultConfig {
    // 视频配置
    struct Video {
        int width = 1920;
        int height = 1080;
        int fps = 30;
        int bitrate = 4000000;  // 4Mbps
        std::string codec = "h264_nvenc";
    };
    
    // 音频配置
    struct Audio {
        int sampleRate = 48000;
        int channels = 2;
        int bitrate = 128000;  // 128kbps
        std::string codec = "aac";
    };
    
    // 网络配置
    struct Network {
        int port = 8080;
        int maxConnections = 10;
        int bufferSize = 1024 * 1024;  // 1MB
    };
    
    // CUDA配置
    struct Cuda {
        int deviceId = 0;
        bool enableHardwareDecode = true;
        bool enableHardwareEncode = true;
    };
    
    Video video;
    Audio audio;
    Network network;
    Cuda cuda;
};

} // namespace StreamKit 