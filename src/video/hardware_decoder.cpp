#include "video/hardware_decoder.hpp"
#include "common/logger.hpp"
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <vector>
#include <fmt/ranges.h>

namespace StreamKit {

// 硬件解码器名称映射（静态成员初始化）
const std::map<AVCodecID, std::vector<std::string>> HardwareVideoDecoder::codec_hw_decoder_map_ = {
    {AV_CODEC_ID_H264, {"h264_cuvid", "h264_qsv", "h264_dxva2", "h264_d3d11va"}},
    {AV_CODEC_ID_HEVC, {"hevc_cuvid", "hevc_qsv", "hevc_dxva2", "hevc_d3d11va"}},
    {AV_CODEC_ID_VP9, {"vp9_cuvid", "vp9_d3d11va"}},
    {AV_CODEC_ID_AV1, {"av1_cuvid", "av1_d3d11va"}},
    {AV_CODEC_ID_MPEG2VIDEO, {"mpeg2_cuvid", "mpeg2_qsv", "mpeg2_dxva2", "mpeg2_d3d11va"}},
    {AV_CODEC_ID_VC1, {"vc1_cuvid", "vc1_dxva2", "vc1_d3d11va"}},
    {AV_CODEC_ID_WMV3, {"wmv3_dxva2", "wmv3_d3d11va"}}
};

HardwareVideoDecoder::HardwareVideoDecoder(HardwareDecoderType hw_type)
    : VideoDecoderBase()
    , hw_type_(hw_type)
    , hw_device_ctx_(nullptr)
    , hw_transfer_frame_(nullptr) {
    
    stats_.decoder_name = getHardwareDecoderName(hw_type);
    stats_.is_hardware_accelerated = true;
}

HardwareVideoDecoder::~HardwareVideoDecoder() {
    close();
}

bool HardwareVideoDecoder::initialize(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("Hardware decoder already initialized");
        return true;
    }
    
    LOG_INFO("Initializing hardware video decoder ({}) for file: {}", 
             getHardwareDecoderName(hw_type_), file_path);
    
    // 1. 创建硬件设备上下文
    if (!createHardwareDevice()) {
        LOG_ERROR("Failed to create hardware device context");
        return false;
    }
    
    // 2. 打开媒体文件
    if (!openMediaFile(file_path)) {
        LOG_ERROR("Failed to open media file");
        return false;
    }
    
    // 3. 初始化流
    if (!initializeStreams()) {
        LOG_ERROR("Failed to initialize streams");
        return false;
    }
    
    // 4. 设置硬件解码器
    if (!setupHardwareDecoder()) {
        LOG_ERROR("Failed to setup hardware decoder");
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("Hardware video decoder initialized successfully");
    
    return true;
}

AVFrame* HardwareVideoDecoder::decodeNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || eof_) {
        return nullptr;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (av_read_frame(format_ctx_, &packet_) >= 0) {
        if (packet_.stream_index == video_stream_index_) {
            AVFrame* frame = performHardwareDecode(&packet_);
            av_packet_unref(&packet_);
            
            if (frame) {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                updateStats(duration.count() / 1000.0);
                
                current_frame_ = frame;
                updateCurrentPosition(current_frame_number_ + 1);
                return frame;
            }
        }
        av_packet_unref(&packet_);
    }
    
    eof_ = true;
    return nullptr;
}

bool HardwareVideoDecoder::seekToFrame(int frame_number) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || frame_number < 0) {
        return false;
    }
    
    // 如果还没有建立帧索引，先建立
    if (!index_built_) {
        if (!buildFrameIndex()) {
            LOG_ERROR("Failed to build frame index for seeking");
            return false;
        }
    }
    
    if (frame_number >= static_cast<int>(frame_index_.size())) {
        LOG_ERROR("Frame number {} out of range (max: {})", frame_number, frame_index_.size() - 1);
        return false;
    }
    
    const FrameInfo& frame_info = frame_index_[frame_number];
    return seekToPosition(frame_info.pts);
}

bool HardwareVideoDecoder::seekToTime(double time_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || time_seconds < 0) {
        return false;
    }
    
    // 将时间转换为时间戳
    int64_t timestamp = static_cast<int64_t>(time_seconds * AV_TIME_BASE);
    
    if (video_codec_ctx_ && video_codec_ctx_->time_base.den > 0) {
        timestamp = av_rescale_q(timestamp, AV_TIME_BASE_Q, video_codec_ctx_->time_base);
    }
    
    return seekToPosition(timestamp);
}

void HardwareVideoDecoder::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cleanupHardwareResources();
    
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
    
    initialized_ = false;
    eof_ = false;
    
    LOG_INFO("Hardware video decoder closed");
}

DecoderType HardwareVideoDecoder::getDecoderType() const {
    return hardwareTypeToDecoderType(hw_type_);
}

std::string HardwareVideoDecoder::getDecoderName() const {
    if (video_codec_ctx_ && video_codec_ctx_->codec) {
        return std::string("Hardware ") + video_codec_ctx_->codec->name + " (" + getHardwareDecoderName(hw_type_) + ")";
    }
    return std::string("Hardware Decoder (") + getHardwareDecoderName(hw_type_) + ")";
}

AVFrame* HardwareVideoDecoder::transferToSystemMemory(AVFrame* hw_frame) {
    if (!hw_frame || !hw_transfer_frame_) {
        return hw_frame;
    }
    
    // 检查是否是硬件帧
    if (hw_frame->format != AV_PIX_FMT_CUDA && 
        hw_frame->format != AV_PIX_FMT_D3D11 &&
        hw_frame->format != AV_PIX_FMT_DXVA2_VLD &&
        hw_frame->format != AV_PIX_FMT_QSV) {
        // 已经是系统内存中的帧
        return hw_frame;
    }
    
    // 传输硬件帧到系统内存
    int ret = av_hwframe_transfer_data(hw_transfer_frame_, hw_frame, 0);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to transfer hardware frame to system memory: {}", errbuf);
        return hw_frame;
    }
    
    // 复制时间戳
    hw_transfer_frame_->pts = hw_frame->pts;
    hw_transfer_frame_->pkt_dts = hw_frame->pkt_dts;
    
    // 释放硬件帧
    av_frame_free(&hw_frame);
    
    return hw_transfer_frame_;
}

bool HardwareVideoDecoder::canTransferToSystemMemory() const {
    return hw_transfer_frame_ != nullptr;
}

std::string HardwareVideoDecoder::getHardwareDeviceInfo() const {
    if (!hw_device_ctx_) {
        return "No hardware device";
    }
    
    AVHWDeviceContext* device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx_->data);
    return std::string("Hardware device type: ") + av_hwdevice_get_type_name(device_ctx->type);
}

bool HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType hw_type) {
    AVHWDeviceType av_type = getAVHWDeviceType(hw_type);
    if (av_type == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }
    
    // 尝试创建硬件设备上下文来测试支持
    AVBufferRef* test_ctx = nullptr;
    int ret = av_hwdevice_ctx_create(&test_ctx, av_type, nullptr, nullptr, 0);
    
    if (test_ctx) {
        av_buffer_unref(&test_ctx);
    }
    
    return ret >= 0;
}

// 私有方法实现
bool HardwareVideoDecoder::initializeHardwareContext() {
    return createHardwareDevice();
}

bool HardwareVideoDecoder::setupHardwareDecoder() {
    if (video_stream_index_ < 0) {
        LOG_ERROR("No video stream found");
        return false;
    }
    
    AVStream* video_stream = format_ctx_->streams[video_stream_index_];
    
    // 查找硬件解码器
    const AVCodec* software_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!software_codec) {
        LOG_ERROR("Unsupported video codec: {}", static_cast<int>(video_stream->codecpar->codec_id));
        return false;
    }
    
    const AVCodec* hw_codec = findHardwareDecoder(software_codec);
    if (!hw_codec) {
        LOG_ERROR("Hardware decoder not found for codec: {}", software_codec->name);
        return false;
    }
    
    LOG_INFO("Using hardware video codec: {}", hw_codec->name);
    
    // 分配解码器上下文
    video_codec_ctx_ = avcodec_alloc_context3(hw_codec);
    if (!video_codec_ctx_) {
        LOG_ERROR("Failed to allocate video codec context");
        return false;
    }
    
    // 复制流参数到解码器上下文
    int ret = avcodec_parameters_to_context(video_codec_ctx_, video_stream->codecpar);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to copy codec parameters: {}", errbuf);
        return false;
    }
    
    // 设置硬件设备上下文
    video_codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
    
    // 打开解码器
    ret = avcodec_open2(video_codec_ctx_, hw_codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to open hardware codec: {}", errbuf);
        return false;
    }
    
    // 分配硬件帧传输缓冲区
    hw_transfer_frame_ = av_frame_alloc();
    if (!hw_transfer_frame_) {
        LOG_ERROR("Failed to allocate hardware transfer frame");
        return false;
    }
    
    LOG_INFO("Hardware decoder opened successfully - {}x{}, {} fps", 
             video_codec_ctx_->width, video_codec_ctx_->height, getVideoFPS());
    
    return true;
}

const AVCodec* HardwareVideoDecoder::findHardwareDecoder(const AVCodec* software_codec) {
    AVCodecID codec_id = software_codec->id;

    // DEBUG: 列出 FFmpeg 中所有可用的该 codec ID 的解码器
    LOG_DEBUG("Searching for hardware decoder. Codec: {} (ID: {}), HW Type: {}",
              software_codec->name, static_cast<int>(codec_id), getHardwareDecoderName(hw_type_));

    void* it = nullptr;
    const AVCodec* c = nullptr;
    std::vector<std::string> available_decoders;
    while ((c = av_codec_iterate(&it))) {
        if (c->id == codec_id && av_codec_is_decoder(c)) {
            available_decoders.push_back(c->name);
            LOG_DEBUG("  Available decoder: {}", c->name);
        }
    }

    // 检查是否有对应的硬件解码器
    auto map_it = codec_hw_decoder_map_.find(codec_id);
    if (map_it == codec_hw_decoder_map_.end()) {
        LOG_WARN("No hardware decoder mapping found for codec: {}", software_codec->name);
        return nullptr;
    }

    // 根据当前硬件类型查找对应的解码器
    std::string hw_decoder_name = getHardwareDecoderName(hw_type_);
    LOG_DEBUG("Looking for decoders containing: {}", hw_decoder_name);

    for (const std::string& decoder_name : map_it->second) {
        LOG_DEBUG("  Trying to find: {}", decoder_name);
        if (decoder_name.find(hw_decoder_name) != std::string::npos) {
            const AVCodec* hw_codec = avcodec_find_decoder_by_name(decoder_name.c_str());
            if (hw_codec) {
                LOG_INFO("Found hardware decoder: {}", decoder_name);
                return hw_codec;
            } else {
                LOG_WARN("  Decoder {} not found in FFmpeg build", decoder_name);
            }
        }
    }

    // Join available decoders into a string for logging
    std::string available_list;
    for (size_t i = 0; i < available_decoders.size(); ++i) {
        if (i > 0) available_list += ", ";
        available_list += available_decoders[i];
    }

    LOG_WARN("Hardware decoder not found for {} on {}", software_codec->name, hw_decoder_name);
    LOG_WARN("Available decoders for this codec: {}", available_list);
    return nullptr;
}

bool HardwareVideoDecoder::supportsHardwareDecoding(AVCodecID codec_id) const {
    auto it = codec_hw_decoder_map_.find(codec_id);
    return it != codec_hw_decoder_map_.end();
}

AVFrame* HardwareVideoDecoder::performHardwareDecode(AVPacket* packet) {
    // 发送包到解码器
    int ret = avcodec_send_packet(video_codec_ctx_, packet);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Error sending packet to hardware decoder: {}", errbuf);
        return nullptr;
    }
    
    // 接收解码后的帧
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("Failed to allocate frame for hardware decoding");
        return nullptr;
    }
    
    ret = avcodec_receive_frame(video_codec_ctx_, frame);
    if (ret == 0) {
        // 成功解码
        return frame;
    } else if (ret == AVERROR(EAGAIN)) {
        // 需要更多输入数据
        av_frame_free(&frame);
        return nullptr;
    } else if (ret == AVERROR_EOF) {
        // 解码器已到达文件末尾
        av_frame_free(&frame);
        eof_ = true;
        return nullptr;
    } else {
        // 解码错误
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Error receiving frame from hardware decoder: {}", errbuf);
        av_frame_free(&frame);
        return nullptr;
    }
}

AVHWDeviceType HardwareVideoDecoder::getAVHWDeviceType(HardwareDecoderType hw_type) {
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

std::string HardwareVideoDecoder::getHardwareDecoderName(HardwareDecoderType hw_type) {
    switch (hw_type) {
        case HardwareDecoderType::CUDA: return "cuvid";
        case HardwareDecoderType::DXVA2: return "dxva2";
        case HardwareDecoderType::D3D11VA: return "d3d11va";
        case HardwareDecoderType::VAAPI: return "vaapi";
        case HardwareDecoderType::QSV: return "qsv";
        case HardwareDecoderType::VIDEOTOOLBOX: return "videotoolbox";
        default: return "none";
    }
}

bool HardwareVideoDecoder::createHardwareDevice() {
    AVHWDeviceType device_type = getAVHWDeviceType(hw_type_);
    if (device_type == AV_HWDEVICE_TYPE_NONE) {
        LOG_ERROR("Invalid hardware device type");
        return false;
    }
    
    int ret = av_hwdevice_ctx_create(&hw_device_ctx_, device_type, nullptr, nullptr, 0);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to create hardware device context for {}: {}", 
                  av_hwdevice_get_type_name(device_type), errbuf);
        return false;
    }
    
    LOG_INFO("Created hardware device context for: {}", av_hwdevice_get_type_name(device_type));
    return true;
}

void HardwareVideoDecoder::cleanupHardwareResources() {
    if (hw_transfer_frame_) {
        av_frame_free(&hw_transfer_frame_);
    }
    
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
    }
}

} // namespace StreamKit 