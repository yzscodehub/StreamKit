#include "video/timestamp_manager.hpp"
#include "common/logger.hpp"
extern "C" {
#include <libavutil/mathematics.h>
}

namespace StreamKit {

TimestampManager::TimestampManager() {
    global_time_base_ = {1, 90000};  // 90kHz timebase
}

TimestampManager::~TimestampManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    stream_timestamps_.clear();
    timestamp_history_.clear();
}

void TimestampManager::setGlobalTimeBase(AVRational time_base) {
    std::lock_guard<std::mutex> lock(mutex_);
    global_time_base_ = time_base;
    LOG_INFO("Global time base set to {}/{}", time_base.num, time_base.den);
}

void TimestampManager::registerStream(int stream_index, AVRational time_base) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StreamTimestamp& ts = stream_timestamps_[stream_index];
    ts.original_time_base = time_base;
    ts.is_valid = true;
    
    LOG_INFO("Registered stream {} with time base {}/{}", 
             stream_index, time_base.num, time_base.den);
}

int64_t TimestampManager::getCorrectedTimestamp(int64_t timestamp, int stream_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = stream_timestamps_.find(stream_index);
    if (it == stream_timestamps_.end()) {
        LOG_WARN("Stream {} not registered", stream_index);
        return timestamp;
    }
    
    return correctTimestamp(timestamp, stream_index);
}

bool TimestampManager::isAVSync(int64_t audio_pts, int64_t video_pts, int tolerance_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 转换为毫秒
    int64_t tolerance_ts = av_rescale_q(tolerance_ms, {1, 1000}, global_time_base_);
    int64_t diff = std::abs(audio_pts - video_pts);
    
    bool is_sync = diff <= tolerance_ts;
    
    if (!is_sync) {
        LOG_DEBUG("AV sync check failed: audio_pts={}, video_pts={}, diff={}, tolerance={}", 
                  audio_pts, video_pts, diff, tolerance_ts);
    }
    
    return is_sync;
}

void TimestampManager::updateStreamTimestamp(int stream_index, int64_t pts, int64_t dts) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = stream_timestamps_.find(stream_index);
    if (it == stream_timestamps_.end()) {
        LOG_WARN("Stream {} not registered", stream_index);
        return;
    }
    
    StreamTimestamp& ts = it->second;
    ts.original_pts = pts;
    ts.original_dts = dts;
    ts.normalized_pts = normalizeTimestamp(pts, ts.original_time_base);
    ts.normalized_dts = normalizeTimestamp(dts, ts.original_time_base);
    
    // 更新历史记录
    auto& history = timestamp_history_[stream_index];
    history.push(ts.normalized_pts);
    if (history.size() > MAX_HISTORY_SIZE) {
        history.pop();
    }
}

bool TimestampManager::isStreamRegistered(int stream_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stream_timestamps_.find(stream_index) != stream_timestamps_.end();
}

AVRational TimestampManager::getStreamTimeBase(int stream_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = stream_timestamps_.find(stream_index);
    if (it != stream_timestamps_.end()) {
        return it->second.original_time_base;
    }
    
    return {0, 1};
}

void TimestampManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    stream_timestamps_.clear();
    timestamp_history_.clear();
    LOG_INFO("Timestamp manager reset");
}

int64_t TimestampManager::normalizeTimestamp(int64_t timestamp, AVRational time_base) {
    if (time_base.num == 0 || time_base.den == 0) {
        return timestamp;
    }
    
    return av_rescale_q(timestamp, time_base, global_time_base_);
}

int64_t TimestampManager::correctTimestamp(int64_t timestamp, int stream_index) {
    auto it = stream_timestamps_.find(stream_index);
    if (it == stream_timestamps_.end()) {
        return timestamp;
    }
    
    const StreamTimestamp& ts = it->second;
    if (!ts.is_valid) {
        return timestamp;
    }
    
    // 基础时间戳转换
    int64_t normalized = normalizeTimestamp(timestamp, ts.original_time_base);
    
    // 简单的线性校正（可以根据需要实现更复杂的校正算法）
    auto& history = timestamp_history_[stream_index];
    if (history.size() >= 2) {
        // 计算时间戳变化趋势
        std::queue<int64_t> temp_history = history;
        int64_t prev = temp_history.front();
        temp_history.pop();
        int64_t curr = temp_history.front();
        
        int64_t expected_diff = curr - prev;
        int64_t actual_diff = normalized - ts.normalized_pts;
        
        // 如果差异过大，进行校正
        if (std::abs(actual_diff - expected_diff) > expected_diff / 2) {
            normalized = ts.normalized_pts + expected_diff;
            LOG_DEBUG("Timestamp corrected for stream {}: {} -> {}", 
                      stream_index, timestamp, normalized);
        }
    }
    
    return normalized;
}

} // namespace StreamKit 