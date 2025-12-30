#include "video/software_decoder.hpp"
#include "common/logger.hpp"
#include <libavutil/error.h>

namespace StreamKit {

SoftwareVideoDecoder::SoftwareVideoDecoder()
    : VideoDecoderBase()
    , sws_ctx_(nullptr)
    , converted_frame_(nullptr)
    , output_pix_fmt_(AV_PIX_FMT_YUV420P)
    , convert_width_(0)
    , convert_height_(0)
    , convert_pix_fmt_(AV_PIX_FMT_NONE)
    , format_conversion_enabled_(false) {
    
    stats_.decoder_name = "Software Decoder";
    stats_.is_hardware_accelerated = false;
}

SoftwareVideoDecoder::~SoftwareVideoDecoder() {
    close();
}

bool SoftwareVideoDecoder::initialize(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("Software decoder already initialized");
        return true;
    }
    
    LOG_INFO("Initializing software video decoder for file: {}", file_path);
    
    // 1. 打开媒体文件
    if (!openMediaFile(file_path)) {
        LOG_ERROR("Failed to open media file");
        return false;
    }
    
    // 2. 初始化流
    if (!initializeStreams()) {
        LOG_ERROR("Failed to initialize streams");
        return false;
    }
    
    // 3. 初始化软件解码器
    if (!initializeSoftwareDecoder()) {
        LOG_ERROR("Failed to initialize software decoder");
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("Software video decoder initialized successfully");
    
    return true;
}

AVFrame* SoftwareVideoDecoder::decodeNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || eof_) {
        return nullptr;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (av_read_frame(format_ctx_, &packet_) >= 0) {
        if (packet_.stream_index == video_stream_index_) {
            AVFrame* frame = performSoftwareDecode(&packet_);
            av_packet_unref(&packet_);
            
            if (frame) {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                updateStats(duration.count() / 1000.0);
                
                // 如果启用了格式转换，则转换帧格式
                if (format_conversion_enabled_) {
                    return convertFrameFormat(frame);
                }
                
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

bool SoftwareVideoDecoder::seekToFrame(int frame_number) {
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

bool SoftwareVideoDecoder::seekToTime(double time_seconds) {
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

void SoftwareVideoDecoder::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (converted_frame_) {
        av_frame_free(&converted_frame_);
    }
    
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
    format_conversion_enabled_ = false;
    
    LOG_INFO("Software video decoder closed");
}

std::string SoftwareVideoDecoder::getDecoderName() const {
    if (video_codec_ctx_ && video_codec_ctx_->codec) {
        return std::string("Software ") + video_codec_ctx_->codec->name;
    }
    return "Software Decoder";
}

bool SoftwareVideoDecoder::setOutputPixelFormat(AVPixelFormat pix_fmt) {
    std::lock_guard<std::mutex> lock(mutex_);
    output_pix_fmt_ = pix_fmt;
    return true;
}

bool SoftwareVideoDecoder::enableFormatConversion(int width, int height, AVPixelFormat pix_fmt) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("Cannot enable format conversion: decoder not initialized");
        return false;
    }
    
    convert_width_ = width;
    convert_height_ = height;
    convert_pix_fmt_ = pix_fmt;
    
    // 释放旧的转换上下文
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
    }
    
    // 创建新的转换上下文
    sws_ctx_ = sws_getContext(
        video_codec_ctx_->width, video_codec_ctx_->height, video_codec_ctx_->pix_fmt,
        width, height, pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx_) {
        LOG_ERROR("Failed to create SwsContext for format conversion");
        return false;
    }
    
    // 分配转换后的帧
    if (converted_frame_) {
        av_frame_free(&converted_frame_);
    }
    
    converted_frame_ = av_frame_alloc();
    if (!converted_frame_) {
        LOG_ERROR("Failed to allocate converted frame");
        return false;
    }
    
    converted_frame_->width = width;
    converted_frame_->height = height;
    converted_frame_->format = pix_fmt;
    
    int ret = av_frame_get_buffer(converted_frame_, 32);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to allocate converted frame buffer: {}", errbuf);
        return false;
    }
    
    format_conversion_enabled_ = true;
    LOG_INFO("Format conversion enabled: {}x{} -> {}x{}", 
             video_codec_ctx_->width, video_codec_ctx_->height, width, height);
    
    return true;
}

void SoftwareVideoDecoder::disableFormatConversion() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (converted_frame_) {
        av_frame_free(&converted_frame_);
    }
    
    format_conversion_enabled_ = false;
    LOG_INFO("Format conversion disabled");
}

// 私有方法实现
bool SoftwareVideoDecoder::initializeSoftwareDecoder() {
    // 打开视频解码器
    if (video_stream_index_ >= 0) {
        if (!openVideoDecoder()) {
            return false;
        }
    }
    
    // 打开音频解码器
    if (audio_stream_index_ >= 0) {
        if (!openAudioDecoder()) {
            LOG_WARN("Failed to open audio decoder, continuing with video only");
        }
    }
    
    return true;
}

bool SoftwareVideoDecoder::openVideoDecoder() {
    AVStream* video_stream = format_ctx_->streams[video_stream_index_];
    
    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        LOG_ERROR("Unsupported video codec: {}", static_cast<int>(video_stream->codecpar->codec_id));
        return false;
    }
    
    LOG_INFO("Using software video codec: {}", codec->name);
    
    // 分配解码器上下文
    video_codec_ctx_ = avcodec_alloc_context3(codec);
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
    
    // 打开解码器
    ret = avcodec_open2(video_codec_ctx_, codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to open video codec: {}", errbuf);
        return false;
    }
    
    LOG_INFO("Video decoder opened successfully - {}x{}, {} fps", 
             video_codec_ctx_->width, video_codec_ctx_->height, getVideoFPS());
    
    return true;
}

bool SoftwareVideoDecoder::openAudioDecoder() {
    AVStream* audio_stream = format_ctx_->streams[audio_stream_index_];
    
    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!codec) {
        LOG_ERROR("Unsupported audio codec: {}", static_cast<int>(audio_stream->codecpar->codec_id));
        return false;
    }
    
    LOG_INFO("Using software audio codec: {}", codec->name);
    
    // 分配解码器上下文
    audio_codec_ctx_ = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx_) {
        LOG_ERROR("Failed to allocate audio codec context");
        return false;
    }
    
    // 复制流参数到解码器上下文
    int ret = avcodec_parameters_to_context(audio_codec_ctx_, audio_stream->codecpar);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to copy audio codec parameters: {}", errbuf);
        return false;
    }
    
    // 打开解码器
    ret = avcodec_open2(audio_codec_ctx_, codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Failed to open audio codec: {}", errbuf);
        return false;
    }
    
    LOG_INFO("Audio decoder opened successfully - {} Hz, {} channels", 
             audio_codec_ctx_->sample_rate, audio_codec_ctx_->ch_layout.nb_channels);
    
    return true;
}

AVFrame* SoftwareVideoDecoder::performSoftwareDecode(AVPacket* packet) {
    // 发送包到解码器
    int ret = avcodec_send_packet(video_codec_ctx_, packet);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR("Error sending packet to decoder: {}", errbuf);
        return nullptr;
    }
    
    // 接收解码后的帧
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("Failed to allocate frame");
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
        LOG_ERROR("Error receiving frame from decoder: {}", errbuf);
        av_frame_free(&frame);
        return nullptr;
    }
}

AVFrame* SoftwareVideoDecoder::convertFrameFormat(AVFrame* input_frame) {
    if (!sws_ctx_ || !converted_frame_) {
        return input_frame;
    }
    
    // 执行格式转换
    int ret = sws_scale(
        sws_ctx_,
        input_frame->data, input_frame->linesize, 0, input_frame->height,
        converted_frame_->data, converted_frame_->linesize
    );
    
    if (ret < 0) {
        LOG_ERROR("Error during format conversion");
        return input_frame;
    }
    
    // 复制时间戳和其他元数据
    converted_frame_->pts = input_frame->pts;
    converted_frame_->pkt_dts = input_frame->pkt_dts;
    
    // 释放原始帧
    av_frame_free(&input_frame);
    
    return converted_frame_;
}

} // namespace StreamKit