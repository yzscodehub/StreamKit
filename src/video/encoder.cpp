#include "video/encoder.hpp"
#include "common/logger.hpp"
#include <libavutil/opt.h>

namespace StreamKit {

VideoEncoder::VideoEncoder()
    : encoder_ctx_(nullptr)
    , hw_frames_ctx_(nullptr)
    , width_(0)
    , height_(0)
    , fps_(0)
    , bitrate_(0)
    , gop_(0)
    , initialized_(false)
    , frame_count_(0)
    , total_bytes_(0) {
    
    av_init_packet(&packet_);
}

VideoEncoder::~VideoEncoder() {
    close();
}

bool VideoEncoder::initialize(int width, int height, int fps, int bitrate) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("Encoder already initialized");
        return true;
    }
    
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = bitrate;
    gop_ = fps * 2;  // 2秒GOP
    
    return createEncoderContext();
}

bool VideoEncoder::initNvencEncoder(int width, int height, int fps) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("Encoder already initialized");
        return true;
    }
    
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = 4000000;  // 4Mbps default
    gop_ = fps * 2;  // 2秒GOP
    
    return createEncoderContext();
}

AVPacket* VideoEncoder::encodeFrame(AVFrame* frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !encoder_ctx_ || !frame) {
        return nullptr;
    }
    
    // 发送帧到编码器
    int ret = avcodec_send_frame(encoder_ctx_, frame);
    if (ret < 0) {
        LOG_ERROR("Error sending frame to encoder: {}", av_err2str(ret));
        return nullptr;
    }
    
    // 接收编码包
    ret = avcodec_receive_packet(encoder_ctx_, &packet_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        LOG_ERROR("Error receiving packet from encoder: {}", av_err2str(ret));
        return nullptr;
    }
    
    // 创建新的包
    AVPacket* encoded_packet = av_packet_alloc();
    av_packet_ref(encoded_packet, &packet_);
    
    // 更新统计
    frame_count_++;
    total_bytes_ += encoded_packet->size;
    
    LOG_DEBUG("Encoded frame {}: {} bytes", frame_count_, encoded_packet->size);
    
    return encoded_packet;
}

std::vector<AVPacket*> VideoEncoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<AVPacket*> packets;
    
    if (!initialized_ || !encoder_ctx_) {
        return packets;
    }
    
    // 发送空帧表示结束
    int ret = avcodec_send_frame(encoder_ctx_, nullptr);
    if (ret < 0) {
        LOG_ERROR("Error sending null frame to encoder: {}", av_err2str(ret));
        return packets;
    }
    
    // 接收所有剩余的包
    while (ret >= 0) {
        ret = avcodec_receive_packet(encoder_ctx_, &packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_ERROR("Error receiving packet from encoder: {}", av_err2str(ret));
            break;
        }
        
        AVPacket* encoded_packet = av_packet_alloc();
        av_packet_ref(encoded_packet, &packet_);
        packets.push_back(encoded_packet);
        
        frame_count_++;
        total_bytes_ += encoded_packet->size;
    }
    
    LOG_INFO("Flushed {} packets from encoder", packets.size());
    return packets;
}

void VideoEncoder::setBitrate(int bitrate) {
    std::lock_guard<std::mutex> lock(mutex_);
    bitrate_ = bitrate;
    
    if (encoder_ctx_) {
        encoder_ctx_->bit_rate = bitrate;
        LOG_INFO("Bitrate set to {} bps", bitrate);
    }
}

void VideoEncoder::setFPS(int fps) {
    std::lock_guard<std::mutex> lock(mutex_);
    fps_ = fps;
    
    if (encoder_ctx_) {
        encoder_ctx_->time_base.num = 1;
        encoder_ctx_->time_base.den = fps;
        encoder_ctx_->framerate.num = fps;
        encoder_ctx_->framerate.den = 1;
        LOG_INFO("FPS set to {}", fps);
    }
}

void VideoEncoder::setGOP(int gop) {
    std::lock_guard<std::mutex> lock(mutex_);
    gop_ = gop;
    
    if (encoder_ctx_) {
        encoder_ctx_->gop_size = gop;
        LOG_INFO("GOP size set to {}", gop);
    }
}

void VideoEncoder::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (encoder_ctx_) {
        avcodec_free_context(&encoder_ctx_);
    }
    
    if (hw_frames_ctx_) {
        av_buffer_unref(&hw_frames_ctx_);
    }
    
    initialized_ = false;
    
    LOG_INFO("Video encoder closed. Total frames: {}, Total bytes: {}", 
             frame_count_, total_bytes_);
}

bool VideoEncoder::createEncoderContext() {
    // 查找NVENC编码器
    const AVCodec* encoder = avcodec_find_encoder_by_name("h264_nvenc");
    if (!encoder) {
        LOG_ERROR("NVENC encoder not found");
        return false;
    }
    
    encoder_ctx_ = avcodec_alloc_context3(encoder);
    if (!encoder_ctx_) {
        LOG_ERROR("Failed to allocate encoder context");
        return false;
    }
    
    return setEncoderParameters();
}

bool VideoEncoder::setEncoderParameters() {
    // 基本参数
    encoder_ctx_->width = width_;
    encoder_ctx_->height = height_;
    encoder_ctx_->time_base.num = 1;
    encoder_ctx_->time_base.den = fps_;
    encoder_ctx_->framerate.num = fps_;
    encoder_ctx_->framerate.den = 1;
    encoder_ctx_->bit_rate = bitrate_;
    encoder_ctx_->gop_size = gop_;
    
    // 像素格式
    encoder_ctx_->pix_fmt = AV_PIX_FMT_NV12;
    
    // 编码预设
    av_opt_set(encoder_ctx_->priv_data, "preset", "fast", 0);
    av_opt_set(encoder_ctx_->priv_data, "tune", "zerolatency", 0);
    av_opt_set(encoder_ctx_->priv_data, "profile", "main", 0);
    av_opt_set(encoder_ctx_->priv_data, "level", "4.1", 0);
    
    // 其他参数
    encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    encoder_ctx_->max_b_frames = 0;  // 低延迟
    
    // 打开编码器
    int ret = avcodec_open2(encoder_ctx_, nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to open encoder: {}", av_err2str(ret));
        avcodec_free_context(&encoder_ctx_);
        return false;
    }
    
    initialized_ = true;
    
    LOG_INFO("NVENC encoder initialized: {}x{}, {}fps, {}bps", 
             width_, height_, fps_, bitrate_);
    
    return true;
}

bool VideoEncoder::createHardwareFrameContext() {
    // 创建硬件帧上下文
    hw_frames_ctx_ = av_hwframe_ctx_alloc(encoder_ctx_->hw_device_ctx);
    if (!hw_frames_ctx_) {
        LOG_ERROR("Failed to allocate hardware frame context");
        return false;
    }
    
    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ctx_->data;
    frames_ctx->format = AV_PIX_FMT_CUDA;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width = width_;
    frames_ctx->height = height_;
    frames_ctx->initial_pool_size = 20;
    
    int ret = av_hwframe_ctx_init(hw_frames_ctx_);
    if (ret < 0) {
        LOG_ERROR("Failed to initialize hardware frame context: {}", av_err2str(ret));
        av_buffer_unref(&hw_frames_ctx_);
        return false;
    }
    
    encoder_ctx_->hw_frames_ctx = av_buffer_ref(hw_frames_ctx_);
    
    LOG_INFO("Hardware frame context created");
    return true;
}

} // namespace StreamKit 