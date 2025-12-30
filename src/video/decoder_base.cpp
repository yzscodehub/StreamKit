#include "video/decoder_base.hpp"
#include "common/logger.hpp"
#include <libavutil/error.h>

namespace StreamKit {

// 前向声明辅助函数
static std::string getHardwareDecoderName(HardwareDecoderType hw_type);
static AVHWDeviceType getAVHWDeviceType(HardwareDecoderType hw_type);

VideoDecoderBase::VideoDecoderBase()
    : format_ctx_(nullptr)
    , video_codec_ctx_(nullptr)
    , audio_codec_ctx_(nullptr)
    , video_stream_index_(-1)
    , audio_stream_index_(-1)
    , initialized_(false)
    , eof_(false)
    , index_built_(false)
    , current_frame_number_(-1)
    , current_frame_(nullptr) {
    
    av_init_packet(&packet_);
    
    // 初始化统计信息
    stats_.total_frames_decoded = 0;
    stats_.total_bytes_processed = 0;
    stats_.average_decode_time_ms = 0.0;
    stats_.fps = 0.0;
    stats_.decoder_name = "Unknown";
    stats_.is_hardware_accelerated = false;
}

VideoDecoderBase::~VideoDecoderBase() {
    close();
}

// 帧访问接口的默认实现
AVFrame* VideoDecoderBase::getFrameAt(int frame_number) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || frame_number < 0) {
        return nullptr;
    }
    
    if (frame_number == current_frame_number_ && current_frame_) {
        return current_frame_;
    }
    
    if (!seekToFrame(frame_number)) {
        return nullptr;
    }
    
    return decodeNextFrame();
}

AVFrame* VideoDecoderBase::getFrameAtTime(double time_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || time_seconds < 0) {
        return nullptr;
    }
    
    if (!seekToTime(time_seconds)) {
        return nullptr;
    }
    
    return decodeNextFrame();
}

AVFrame* VideoDecoderBase::getCurrentFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_frame_;
}

AVFrame* VideoDecoderBase::getNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || eof_) {
        return nullptr;
    }
    
    AVFrame* frame = decodeNextFrame();
    if (frame) {
        updateCurrentPosition(current_frame_number_ + 1);
    }
    
    return frame;
}

AVFrame* VideoDecoderBase::getPreviousFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || current_frame_number_ <= 0) {
        return nullptr;
    }
    
    int target_frame = current_frame_number_ - 1;
    if (!seekToFrame(target_frame)) {
        return nullptr;
    }
    
    return decodeNextFrame();
}

// 索引功能的默认实现
bool VideoDecoderBase::buildFrameIndex() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || index_built_) {
        return index_built_;
    }
    
    LOG_INFO("Building frame index for video file...");
    
    // 保存当前位置
    int saved_position = current_frame_number_;
    
    frame_index_.clear();
    
    // 重新开始
    if (!seekToPosition(0)) {
        LOG_ERROR("Failed to seek to beginning for index building");
        return false;
    }
    
    AVPacket packet;
    av_init_packet(&packet);
    
    int frame_count = 0;
    while (av_read_frame(format_ctx_, &packet) >= 0) {
        if (packet.stream_index == video_stream_index_) {
            FrameInfo frame_info;
            frame_info.pts = packet.pts;
            frame_info.dts = packet.dts;
            frame_info.pos = packet.pos;
            frame_info.frame_number = frame_count++;
            frame_info.is_keyframe = (packet.flags & AV_PKT_FLAG_KEY) != 0;
            
            // 计算时间戳
            if (video_codec_ctx_ && video_codec_ctx_->time_base.den > 0) {
                frame_info.timestamp_sec = packet.pts * av_q2d(video_codec_ctx_->time_base);
            } else {
                frame_info.timestamp_sec = 0.0;
            }
            
            frame_index_.push_back(frame_info);
        }
        av_packet_unref(&packet);
    }
    
    index_built_ = true;
    
    LOG_INFO("Frame index built successfully. Total frames: {}", frame_index_.size());
    
    // 恢复之前的位置
    if (saved_position >= 0) {
        seekToFrame(saved_position);
    }
    
    return true;
}

double VideoDecoderBase::getDuration() const {
    if (!initialized_ || !format_ctx_) {
        return 0.0;
    }
    
    if (format_ctx_->duration != AV_NOPTS_VALUE) {
        return static_cast<double>(format_ctx_->duration) / AV_TIME_BASE;
    }
    
    return 0.0;
}

double VideoDecoderBase::getCurrentTime() const {
    if (!initialized_ || current_frame_number_ < 0 || frame_index_.empty()) {
        return 0.0;
    }
    
    if (current_frame_number_ < static_cast<int>(frame_index_.size())) {
        return frame_index_[current_frame_number_].timestamp_sec;
    }
    
    return 0.0;
}

// 视频信息获取
int VideoDecoderBase::getVideoWidth() const {
    return (video_codec_ctx_) ? video_codec_ctx_->width : 0;
}

int VideoDecoderBase::getVideoHeight() const {
    return (video_codec_ctx_) ? video_codec_ctx_->height : 0;
}

int VideoDecoderBase::getVideoFPS() const {
    if (!video_codec_ctx_) return 0;
    
    AVRational fps = av_guess_frame_rate(format_ctx_, 
                                        format_ctx_->streams[video_stream_index_], 
                                        nullptr);
    if (fps.den > 0) {
        return static_cast<int>(av_q2d(fps));
    }
    
    return 0;
}

AVRational VideoDecoderBase::getVideoTimeBase() const {
    if (!video_codec_ctx_) {
        return {0, 1};
    }
    return video_codec_ctx_->time_base;
}

// 音频信息获取
int VideoDecoderBase::getAudioSampleRate() const {
    return (audio_codec_ctx_) ? audio_codec_ctx_->sample_rate : 0;
}

int VideoDecoderBase::getAudioChannels() const {
    return (audio_codec_ctx_) ? audio_codec_ctx_->ch_layout.nb_channels : 0;
}

AVSampleFormat VideoDecoderBase::getAudioFormat() const {
    return (audio_codec_ctx_) ? audio_codec_ctx_->sample_fmt : AV_SAMPLE_FMT_NONE;
}

// 统计信息
DecodingStats VideoDecoderBase::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void VideoDecoderBase::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stats_.total_frames_decoded = 0;
    stats_.total_bytes_processed = 0;
    stats_.average_decode_time_ms = 0.0;
    stats_.fps = 0.0;
}

// 受保护的方法实现
bool VideoDecoderBase::openMediaFile(const std::string& file_path) {
    file_path_ = file_path;
    
    // 打开输入文件
    int ret = avformat_open_input(&format_ctx_, file_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to open input file '{}': {}", file_path, errbuf);
        return false;
    }
    
    // 获取流信息
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to find stream info: {}", errbuf);
        return false;
    }
    
    LOG_INFO("Successfully opened media file: {}", file_path);
    LOG_INFO("Duration: {:.2f} seconds", getDuration());
    
    return true;
}

int VideoDecoderBase::findBestStream(AVMediaType type) {
    if (!format_ctx_) {
        return -1;
    }
    
    return av_find_best_stream(format_ctx_, type, -1, -1, nullptr, 0);
}

bool VideoDecoderBase::initializeStreams() {
    // 查找视频流
    video_stream_index_ = findBestStream(AVMEDIA_TYPE_VIDEO);
    if (video_stream_index_ < 0) {
        LOG_WARN("No video stream found");
    } else {
        LOG_INFO("Found video stream at index {}", video_stream_index_);
    }
    
    // 查找音频流
    audio_stream_index_ = findBestStream(AVMEDIA_TYPE_AUDIO);
    if (audio_stream_index_ < 0) {
        LOG_WARN("No audio stream found");
    } else {
        LOG_INFO("Found audio stream at index {}", audio_stream_index_);
    }
    
    return video_stream_index_ >= 0; // 至少需要有视频流
}

bool VideoDecoderBase::seekToPosition(int64_t timestamp) {
    if (!format_ctx_ || video_stream_index_ < 0) {
        return false;
    }
    
    int ret = av_seek_frame(format_ctx_, video_stream_index_, timestamp, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to seek to timestamp {}: {}", timestamp, errbuf);
        return false;
    }
    
    // 清空解码器缓冲区
    if (video_codec_ctx_) {
        avcodec_flush_buffers(video_codec_ctx_);
    }
    if (audio_codec_ctx_) {
        avcodec_flush_buffers(audio_codec_ctx_);
    }
    
    eof_ = false;
    return true;
}

void VideoDecoderBase::updateCurrentPosition(int frame_number) {
    current_frame_number_ = frame_number;
}

void VideoDecoderBase::updateStats(double decode_time_ms) {
    stats_.total_frames_decoded++;
    stats_.total_bytes_processed += packet_.size;
    
    // 更新平均解码时间
    if (stats_.total_frames_decoded == 1) {
        stats_.average_decode_time_ms = decode_time_ms;
    } else {
        stats_.average_decode_time_ms = 
            (stats_.average_decode_time_ms * (stats_.total_frames_decoded - 1) + decode_time_ms) / 
            stats_.total_frames_decoded;
    }
    
    // 计算FPS
    if (stats_.average_decode_time_ms > 0) {
        stats_.fps = 1000.0 / stats_.average_decode_time_ms;
    }
}

// 静态工具方法
std::vector<HardwareDecoderInfo> VideoDecoderBase::getAvailableHardwareDecoders() {
    std::vector<HardwareDecoderInfo> decoders;
    
    std::vector<std::pair<HardwareDecoderType, std::string>> hw_types = {
        {HardwareDecoderType::CUDA, "NVIDIA CUDA (NVDEC)"},
        {HardwareDecoderType::DXVA2, "DirectX Video Acceleration 2.0"},
        {HardwareDecoderType::D3D11VA, "Direct3D 11 Video Acceleration"},
        {HardwareDecoderType::VAAPI, "Video Acceleration API (Intel/AMD)"},
        {HardwareDecoderType::QSV, "Intel Quick Sync Video"}
    };
    
    for (const auto& [type, desc] : hw_types) {
        HardwareDecoderInfo info;
        info.type = type;
        info.name = getHardwareDecoderName(type);
        info.description = desc;
        
        // 测试硬件解码器是否可用
        AVBufferRef* test_ctx = nullptr;
        AVHWDeviceType av_type = getAVHWDeviceType(type);
        int ret = av_hwdevice_ctx_create(&test_ctx, av_type, nullptr, nullptr, 0);
        info.available = (ret >= 0);
        info.av_type = av_type;
        
        if (test_ctx) {
            av_buffer_unref(&test_ctx);
        }
        
        decoders.push_back(info);
        
        LOG_INFO("Hardware decoder {}: {} ({})", 
                info.name, info.available ? "Available" : "Not available", info.description);
    }
    
    return decoders;
}

void VideoDecoderBase::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_frame_) {
        av_frame_free(&current_frame_);
    }
    
    if (video_codec_ctx_) {
        avcodec_free_context(&video_codec_ctx_);
    }
    
    if (audio_codec_ctx_) {
        avcodec_free_context(&audio_codec_ctx_);
    }
    
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
    }
    
    av_packet_unref(&packet_);
    
    initialized_ = false;
    eof_ = false;
    index_built_ = false;
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
    current_frame_number_ = -1;
    file_path_.clear();
    frame_index_.clear();
    
    LOG_INFO("VideoDecoderBase closed successfully");
}

// 辅助函数
static std::string getHardwareDecoderName(HardwareDecoderType hw_type) {
    switch (hw_type) {
        case HardwareDecoderType::CUDA: return "cuda";
        case HardwareDecoderType::DXVA2: return "dxva2";
        case HardwareDecoderType::D3D11VA: return "d3d11va";
        case HardwareDecoderType::VAAPI: return "vaapi";
        case HardwareDecoderType::QSV: return "qsv";
        case HardwareDecoderType::VIDEOTOOLBOX: return "videotoolbox";
        default: return "none";
    }
}

static AVHWDeviceType getAVHWDeviceType(HardwareDecoderType hw_type) {
    switch (hw_type) {
        case HardwareDecoderType::CUDA: return AV_HWDEVICE_TYPE_CUDA;
        case HardwareDecoderType::DXVA2: return AV_HWDEVICE_TYPE_DXVA2;
        case HardwareDecoderType::D3D11VA: return AV_HWDEVICE_TYPE_D3D11VA;
        case HardwareDecoderType::VAAPI: return AV_HWDEVICE_TYPE_VAAPI;
        case HardwareDecoderType::QSV: return AV_HWDEVICE_TYPE_QSV;
        case HardwareDecoderType::VIDEOTOOLBOX: return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        default: return AV_HWDEVICE_TYPE_NONE;
    }
}

} // namespace StreamKit 