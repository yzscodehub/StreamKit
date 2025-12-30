#pragma once

#include "decoder_base.hpp"
#include <libswscale/swscale.h>

namespace StreamKit {

/**
 * 软件视频解码器
 * 使用CPU进行视频解码
 */
class SoftwareVideoDecoder : public VideoDecoderBase {
public:
    SoftwareVideoDecoder();
    virtual ~SoftwareVideoDecoder();

    // 实现基类的纯虚函数
    bool initialize(const std::string& file_path) override;
    AVFrame* decodeNextFrame() override;
    bool seekToFrame(int frame_number) override;
    bool seekToTime(double time_seconds) override;
    void close() override;

    // 重写基类方法
    DecoderType getDecoderType() const override { return DecoderType::SOFTWARE; }
    std::string getDecoderName() const override;

    // 软件解码器特有功能
    bool setOutputPixelFormat(AVPixelFormat pix_fmt);
    AVPixelFormat getOutputPixelFormat() const { return output_pix_fmt_; }
    
    // 格式转换功能
    bool enableFormatConversion(int width, int height, AVPixelFormat pix_fmt);
    void disableFormatConversion();
    bool isFormatConversionEnabled() const { return sws_ctx_ != nullptr; }

private:
    // 初始化软件解码器
    bool initializeSoftwareDecoder();
    
    // 打开解码器
    bool openVideoDecoder();
    bool openAudioDecoder();
    
    // 解码相关
    AVFrame* performSoftwareDecode(AVPacket* packet);
    AVFrame* convertFrameFormat(AVFrame* input_frame);
    
    // 像素格式转换
    SwsContext* sws_ctx_;
    AVFrame* converted_frame_;
    AVPixelFormat output_pix_fmt_;
    
    // 转换目标参数
    int convert_width_;
    int convert_height_;
    AVPixelFormat convert_pix_fmt_;
    bool format_conversion_enabled_;
};

} // namespace StreamKit 