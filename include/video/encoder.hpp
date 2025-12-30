#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/buffer.h>

namespace StreamKit {

class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();
    
    // 初始化编码器
    bool initialize(int width, int height, int fps, int bitrate);
    
    // 初始化NVENC硬件编码器
    bool initNvencEncoder(int width, int height, int fps);
    
    // 编码帧
    AVPacket* encodeFrame(AVFrame* frame);
    
    // 刷新编码器
    std::vector<AVPacket*> flush();
    
    // 设置编码参数
    void setBitrate(int bitrate);
    void setFPS(int fps);
    void setGOP(int gop);
    
    // 获取编码参数
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getFPS() const { return fps_; }
    int getBitrate() const { return bitrate_; }
    
    // 关闭编码器
    void close();
    
    // 检查是否已初始化
    bool isInitialized() const { return initialized_; }

private:
    // 创建编码器上下文
    bool createEncoderContext();
    
    // 设置编码器参数
    bool setEncoderParameters();
    
    // 创建硬件帧上下文
    bool createHardwareFrameContext();
    
    // 编码器上下文
    AVCodecContext* encoder_ctx_;
    
    // 硬件帧上下文
    AVBufferRef* hw_frames_ctx_;
    
    // 编码参数
    int width_;
    int height_;
    int fps_;
    int bitrate_;
    int gop_;
    
    // 编码包
    AVPacket packet_;
    
    // 状态标志
    bool initialized_;
    
    // 线程安全
    mutable std::mutex mutex_;
    
    // 编码统计
    int64_t frame_count_;
    int64_t total_bytes_;
};

} // namespace StreamKit 