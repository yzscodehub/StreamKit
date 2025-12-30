#pragma once

#include <map>
#include <queue>
#include <mutex>

extern "C" {
#include <libavutil/rational.h>
#include <libavcodec/avcodec.h>
}

namespace StreamKit {

struct StreamTimestamp {
    int64_t original_pts;
    int64_t original_dts;
    int64_t normalized_pts;
    int64_t normalized_dts;
    AVRational original_time_base;
    bool is_valid;
    
    StreamTimestamp() : original_pts(0), original_dts(0), 
                       normalized_pts(0), normalized_dts(0), 
                       original_time_base{0, 1}, is_valid(false) {}
};

class TimestampManager {
public:
    TimestampManager();
    ~TimestampManager();
    
    // 设置全局时间基准
    void setGlobalTimeBase(AVRational time_base);
    
    // 注册流
    void registerStream(int stream_index, AVRational time_base);
    
    // 获取校正后的时间戳
    int64_t getCorrectedTimestamp(int64_t timestamp, int stream_index);
    
    // 音视频同步检查
    bool isAVSync(int64_t audio_pts, int64_t video_pts, int tolerance_ms = 50);
    
    // 更新流时间戳
    void updateStreamTimestamp(int stream_index, int64_t pts, int64_t dts);
    
    // 获取流信息
    bool isStreamRegistered(int stream_index) const;
    AVRational getStreamTimeBase(int stream_index) const;
    
    // 重置时间戳
    void reset();

private:
    // 时间戳转换
    int64_t normalizeTimestamp(int64_t timestamp, AVRational time_base);
    
    // 时间戳校正
    int64_t correctTimestamp(int64_t timestamp, int stream_index);
    
    // 全局时间基准
    AVRational global_time_base_;
    
    // 各流的时间戳映射
    std::map<int, StreamTimestamp> stream_timestamps_;
    
    // 线程安全
    mutable std::mutex mutex_;
    
    // 时间戳历史记录
    std::map<int, std::queue<int64_t>> timestamp_history_;
    static const size_t MAX_HISTORY_SIZE = 100;
};

} // namespace StreamKit 