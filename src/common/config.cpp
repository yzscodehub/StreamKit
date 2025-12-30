#include "common/config.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace StreamKit {

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

void Config::loadFromFile(const std::string& configFile, bool merge) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(configFile);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file: {}", configFile);
        throw std::runtime_error("Failed to open config file: " + configFile);
    }

    try {
        nlohmann::json newConfig;
        file >> newConfig;

        if (merge && !config_.is_null()) {
            // 合并配置：新配置覆盖旧配置的同键值
            config_.merge_patch(newConfig);
        } else {
            config_ = newConfig;
        }

        LOG_INFO("Configuration loaded from file: {}", configFile);
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to parse config file: {} - {}", configFile, e.what());
        throw std::runtime_error("Failed to parse config file: " + std::string(e.what()));
    }
}

void Config::loadFromJson(const nlohmann::json& json, bool merge) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        if (merge && !config_.is_null()) {
            config_.merge_patch(json);
        } else {
            config_ = json;
        }
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to load JSON config: {}", e.what());
        throw;
    }
}

template<typename T>
T Config::get(const std::string& key, const T& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // 支持点号分隔的嵌套访问，如 "video.width"
        nlohmann::json* current = const_cast<nlohmann::json*>(&config_);

        std::stringstream ss(key);
        std::string segment;
        std::vector<std::string> segments;

        while (std::getline(ss, segment, '.')) {
            if (!segment.empty()) {
                segments.push_back(segment);
            }
        }

        for (const auto& seg : segments) {
            if (current->is_object() && current->contains(seg)) {
                current = &(*current)[seg];
            } else {
                return defaultValue;
            }
        }

        return current->get<T>();
    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("Failed to get config key '{}', using default value: {}", key, e.what());
        return defaultValue;
    } catch (...) {
        return defaultValue;
    }
}

template<typename T>
bool Config::set(const std::string& key, const T& value, bool validate) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        nlohmann::json jsonValue = value;
        nlohmann::json oldValue = getByNestedKey(key, false);

        // 验证新值
        if (validate && validator_) {
            if (!validator_(key, jsonValue)) {
                LOG_WARN("Config validation failed for key: {}", key);
                return false;
            }
        }

        // 设置新值
        getByNestedKey(key, true) = jsonValue;

        // 通知监听器
        notifyListeners(key, oldValue, jsonValue);

        return true;
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to set config key '{}': {}", key, e.what());
        return false;
    }
}

bool Config::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        nlohmann::json* current = const_cast<nlohmann::json*>(&config_);

        std::stringstream ss(key);
        std::string segment;

        while (std::getline(ss, segment, '.')) {
            if (!segment.empty()) {
                if (current->is_object() && current->contains(segment)) {
                    current = &(*current)[segment];
                } else {
                    return false;
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool Config::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // 查找父对象和最后一个键
        std::vector<std::string> segments;
        std::stringstream ss(key);
        std::string segment;

        while (std::getline(ss, segment, '.')) {
            if (!segment.empty()) {
                segments.push_back(segment);
            }
        }

        if (segments.empty()) {
            return false;
        }

        nlohmann::json* current = &config_;
        for (size_t i = 0; i < segments.size() - 1; ++i) {
            if (current->is_object() && current->contains(segments[i])) {
                current = &(*current)[segments[i]];
            } else {
                return false;
            }
        }

        if (current->is_object() && current->contains(segments.back())) {
            current->erase(segments.back());
            LOG_INFO("Config key removed: {}", key);
            return true;
        }

        return false;
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to remove config key '{}': {}", key, e.what());
        return false;
    }
}

void Config::saveToFile(const std::string& configFile, bool pretty) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ofstream file(configFile);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file for writing: {}", configFile);
            throw std::runtime_error("Failed to open config file for writing: " + configFile);
        }

        if (pretty) {
            file << config_.dump(4);
        } else {
            file << config_.dump();
        }

        LOG_INFO("Configuration saved to file: {}", configFile);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save config file: {}", e.what());
        throw;
    }
}

nlohmann::json Config::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void Config::setValidator(ConfigValidator validator) {
    std::lock_guard<std::mutex> lock(mutex_);
    validator_ = std::move(validator);
}

size_t Config::addChangeListener(ConfigChangeListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t id = next_listener_id_++;
    listeners_[id] = std::move(listener);
    return id;
}

void Config::removeChangeListener(size_t listenerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.erase(listenerId);
}

std::vector<std::string> Config::getKeys(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;

    try {
        if (prefix.empty()) {
            // 返回顶层键
            if (config_.is_object()) {
                for (auto it = config_.begin(); it != config_.end(); ++it) {
                    keys.push_back(it.key());
                }
            }
        } else {
            // 返回指定前缀下的键
            const nlohmann::json* current = &config_;
            std::vector<std::string> segments;
            std::stringstream ss(prefix);
            std::string segment;

            while (std::getline(ss, segment, '.')) {
                if (!segment.empty()) {
                    segments.push_back(segment);
                }
            }

            for (const auto& seg : segments) {
                if (current->is_object() && current->contains(seg)) {
                    current = &(*current)[seg];
                } else {
                    return keys;
                }
            }

            if (current->is_object()) {
                for (auto it = current->begin(); it != current->end(); ++it) {
                    keys.push_back(prefix + "." + it.key());
                }
            }
        }
    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("Failed to get keys with prefix '{}': {}", prefix, e.what());
    }

    return keys;
}

void Config::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = nlohmann::json::object();
    LOG_INFO("Configuration cleared");
}

void Config::loadDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);

    config_ = {
        {"video", {
            {"width", 1920},
            {"height", 1080},
            {"fps", 30},
            {"bitrate", 4000000},
            {"codec", "h264_nvenc"}
        }},
        {"audio", {
            {"sampleRate", 48000},
            {"channels", 2},
            {"bitrate", 128000},
            {"codec", "aac"}
        }},
        {"network", {
            {"port", 8080},
            {"maxConnections", 10},
            {"bufferSize", 1048576}
        }},
        {"cuda", {
            {"deviceId", 0},
            {"enableHardwareDecode", true},
            {"enableHardwareEncode", true}
        }}
    };

    LOG_INFO("Default configuration loaded");
}

nlohmann::json& Config::getByNestedKey(const std::string& key, bool createMissing) {
    std::vector<std::string> segments;
    std::stringstream ss(key);
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    if (segments.empty()) {
        return config_;
    }

    nlohmann::json* current = &config_;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];

        if (!current->is_object()) {
            if (createMissing) {
                *current = nlohmann::json::object();
            } else {
                throw std::runtime_error("Path does not exist: " + key);
            }
        }

        if (!current->contains(seg)) {
            if (createMissing) {
                (*current)[seg] = nlohmann::json::object();
            } else {
                throw std::runtime_error("Key not found: " + seg);
            }
        }

        current = &(*current)[seg];
    }

    return *current;
}

void Config::notifyListeners(const std::string& key, const nlohmann::json& oldValue, const nlohmann::json& newValue) {
    for (const auto& [id, listener] : listeners_) {
        try {
            listener(key, oldValue, newValue);
        } catch (const std::exception& e) {
            LOG_ERROR("Config change listener threw exception: {}", e.what());
        }
    }
}

// 显式实例化常用类型
template int Config::get<int>(const std::string&, const int&) const;
template double Config::get<double>(const std::string&, const double&) const;
template bool Config::get<bool>(const std::string&, const bool&) const;
template std::string Config::get<std::string>(const std::string&, const std::string&) const;

template bool Config::set<int>(const std::string&, const int&, bool);
template bool Config::set<double>(const std::string&, const double&, bool);
template bool Config::set<bool>(const std::string&, const bool&, bool);
template bool Config::set<std::string>(const std::string&, const std::string&, bool);

} // namespace StreamKit
