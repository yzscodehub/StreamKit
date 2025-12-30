#pragma once

#include "decoder_base.hpp"
#include <libavutil/hwcontext.h>
#include <libavutil/buffer.h>

namespace StreamKit {

/**
 * 硬件视频解码器
 * 使用GPU进行视频解码加速
 */
class HardwareVideoDecoder : public VideoDecoderBase {
public:
    explicit HardwareVideoDecoder(HardwareDecoderType hw_type);
    virtual ~HardwareVideoDecoder();

    // 实现基类的纯虚函数
    bool initialize(const std::string& file_path) override;
    AVFrame* decodeNextFrame() override;
    bool seekToFrame(int frame_number) override;
    bool seekToTime(double time_seconds) override;
    void close() override;

    // 重写基类方法
    DecoderType getDecoderType() const override;
    std::string getDecoderName() const override;

    // 硬件解码器特有功能
    HardwareDecoderType getHardwareType() const { return hw_type_; }
    bool isHardwareDecodingEnabled() const { return hw_device_ctx_ != nullptr; }
    
    // 帧传输功能
    AVFrame* transferToSystemMemory(AVFrame* hw_frame);
    bool canTransferToSystemMemory() const;
    
    // 硬件设备信息
    std::string getHardwareDeviceInfo() const;
    static bool isHardwareTypeSupported(HardwareDecoderType hw_type);

private:
    // 硬件解码器初始化
    bool initializeHardwareContext();
    bool setupHardwareDecoder();
    
    // 查找硬件解码器
    const AVCodec* findHardwareDecoder(const AVCodec* software_codec);
    bool supportsHardwareDecoding(AVCodecID codec_id) const;
    
    // 硬件解码相关
    AVFrame* performHardwareDecode(AVPacket* packet);
    
    // 静态工具方法
    static AVHWDeviceType getAVHWDeviceType(HardwareDecoderType hw_type);
    static std::string getHardwareDecoderName(HardwareDecoderType hw_type);
    
    // 硬件上下文管理
    bool createHardwareDevice();
    void cleanupHardwareResources();
    
    // 硬件解码器类型
    HardwareDecoderType hw_type_;
    
    // 硬件设备上下文
    AVBufferRef* hw_device_ctx_;
    
    // 硬件帧传输
    AVFrame* hw_transfer_frame_;
    
    // 硬件解码器名称映射
    static const std::map<AVCodecID, std::vector<std::string>> codec_hw_decoder_map_;
};

} // namespace StreamKit 