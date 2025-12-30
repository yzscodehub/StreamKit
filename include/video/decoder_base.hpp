#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <map>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/buffer.h>
#include <libswscale/swscale.h>
}

#include "timestamp_manager.hpp"

namespace StreamKit {

// 解码器类型枚举
enum class DecoderType {
    SOFTWARE,
    HARDWARE_CUDA,
    HARDWARE_DXVA2,
    HARDWARE_D3D11VA,
    HARDWARE_VAAPI,
    HARDWARE_QSV,
    HARDWARE_VIDEOTOOLBOX
};

// 硬件解码器类型（保持向后兼容）
enum class HardwareDecoderType {
    NONE,
    CUDA,      // NVIDIA CUDA
    DXVA2,     // DirectX Video Acceleration 2.0
    D3D11VA,   // Direct3D 11 Video Acceleration
    VAAPI,     // Video Acceleration API (Intel/AMD)
    VIDEOTOOLBOX, // Apple VideoToolbox
    QSV        // Intel Quick Sync Video
};

// 硬件解码器信息
struct HardwareDecoderInfo {
    HardwareDecoderType type;
    AVHWDeviceType av_type;
    std::string name;
    bool available;
    std::string description;
};

// 帧信息结构
struct FrameInfo {
    int64_t pts;           // 显示时间戳
    int64_t dts;           // 解码时间戳
    int64_t pos;           // 文件位置
    int frame_number;      // 帧序号
    bool is_keyframe;      // 是否为关键帧
    double timestamp_sec;  // 时间戳（秒）
};

// 解码统计信息
struct DecodingStats {
    int64_t total_frames_decoded;
    int64_t total_bytes_processed;
    double average_decode_time_ms;
    double fps;
    std::string decoder_name;
    bool is_hardware_accelerated;
};

/**
 * 视频解码器抽象基类
 * 定义了所有解码器必须实现的接口
 */
class VideoDecoderBase {
public:
    VideoDecoderBase();
    virtual ~VideoDecoderBase();

    // 纯虚函数 - 子类必须实现
    virtual bool initialize(const std::string& file_path) = 0;
    virtual AVFrame* decodeNextFrame() = 0;
    virtual bool seekToFrame(int frame_number) = 0;
    virtual bool seekToTime(double time_seconds) = 0;
    
    // 虚函数 - 基类提供默认实现，子类可以覆盖
    virtual void close();

    // 公共接口 - 基类提供默认实现
    virtual bool isInitialized() const { return initialized_; }
    virtual bool isEndOfFile() const { return eof_; }
    virtual DecoderType getDecoderType() const = 0;
    virtual std::string getDecoderName() const = 0;
    
    // 帧访问接口
    virtual AVFrame* getFrameAt(int frame_number);
    virtual AVFrame* getFrameAtTime(double time_seconds);
    virtual AVFrame* getCurrentFrame();
    virtual AVFrame* getNextFrame();
    virtual AVFrame* getPreviousFrame();
    
    // 索引功能
    virtual bool buildFrameIndex();
    virtual int getTotalFrames() const { return frame_index_.size(); }
    virtual double getDuration() const;
    virtual std::vector<FrameInfo> getFrameIndex() const { return frame_index_; }
    
    // 位置信息
    virtual int getCurrentFrameNumber() const { return current_frame_number_; }
    virtual double getCurrentTime() const;
    
    // 视频信息
    virtual int getVideoWidth() const;
    virtual int getVideoHeight() const;
    virtual int getVideoFPS() const;
    virtual AVRational getVideoTimeBase() const;
    
    // 音频信息
    virtual int getAudioSampleRate() const;
    virtual int getAudioChannels() const;
    virtual AVSampleFormat getAudioFormat() const;
    
    // 流信息
    virtual bool hasVideoStream() const { return video_stream_index_ >= 0; }
    virtual bool hasAudioStream() const { return audio_stream_index_ >= 0; }
    
    // 统计信息
    virtual DecodingStats getStats() const;
    virtual void resetStats();

    // 静态工具方法
    static std::vector<HardwareDecoderInfo> getAvailableHardwareDecoders();

protected:
    // 受保护的方法供子类使用
    virtual bool openMediaFile(const std::string& file_path);
    virtual int findBestStream(AVMediaType type);
    virtual bool initializeStreams();
    virtual bool seekToPosition(int64_t timestamp);
    virtual void updateCurrentPosition(int frame_number);
    virtual void updateStats(double decode_time_ms);
    
    // 受保护的成员变量
    AVFormatContext* format_ctx_;
    AVCodecContext* video_codec_ctx_;
    AVCodecContext* audio_codec_ctx_;
    
    // 流索引
    int video_stream_index_;
    int audio_stream_index_;
    
    // 状态
    bool initialized_;
    bool eof_;
    bool index_built_;
    std::string file_path_;
    
    // 时间戳管理
    TimestampManager timestamp_manager_;
    
    // 包和帧
    AVPacket packet_;
    
    // 随机访问相关
    std::vector<FrameInfo> frame_index_;
    int current_frame_number_;
    AVFrame* current_frame_;
    
    // 统计信息
    DecodingStats stats_;
    std::chrono::high_resolution_clock::time_point decode_start_time_;
    
    // 线程安全
    mutable std::mutex mutex_;

private:
    // 禁用拷贝构造和赋值
    VideoDecoderBase(const VideoDecoderBase&) = delete;
    VideoDecoderBase& operator=(const VideoDecoderBase&) = delete;
};

// 辅助函数
inline DecoderType hardwareTypeToDecoderType(HardwareDecoderType hw_type) {
    switch (hw_type) {
        case HardwareDecoderType::CUDA: return DecoderType::HARDWARE_CUDA;
        case HardwareDecoderType::DXVA2: return DecoderType::HARDWARE_DXVA2;
        case HardwareDecoderType::D3D11VA: return DecoderType::HARDWARE_D3D11VA;
        case HardwareDecoderType::VAAPI: return DecoderType::HARDWARE_VAAPI;
        case HardwareDecoderType::QSV: return DecoderType::HARDWARE_QSV;
        case HardwareDecoderType::VIDEOTOOLBOX: return DecoderType::HARDWARE_VIDEOTOOLBOX;
        case HardwareDecoderType::NONE:
        default: return DecoderType::SOFTWARE;
    }
}

inline HardwareDecoderType decoderTypeToHardwareType(DecoderType decoder_type) {
    switch (decoder_type) {
        case DecoderType::HARDWARE_CUDA: return HardwareDecoderType::CUDA;
        case DecoderType::HARDWARE_DXVA2: return HardwareDecoderType::DXVA2;
        case DecoderType::HARDWARE_D3D11VA: return HardwareDecoderType::D3D11VA;
        case DecoderType::HARDWARE_VAAPI: return HardwareDecoderType::VAAPI;
        case DecoderType::HARDWARE_QSV: return HardwareDecoderType::QSV;
        case DecoderType::HARDWARE_VIDEOTOOLBOX: return HardwareDecoderType::VIDEOTOOLBOX;
        case DecoderType::SOFTWARE:
        default: return HardwareDecoderType::NONE;
    }
}

} // namespace StreamKit 