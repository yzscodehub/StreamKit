#pragma once

#include "decoder_base.hpp"
#include "software_decoder.hpp"
#include "hardware_decoder.hpp"
#include <memory>

namespace StreamKit {

/**
 * 解码器工厂类
 * 负责创建和管理不同类型的视频解码器
 */
class VideoDecoderFactory {
public:
    // 创建解码器的策略
    enum class CreationStrategy {
        PREFER_HARDWARE,    // 优先使用硬件解码，失败则回退到软件解码
        PREFER_SOFTWARE,    // 优先使用软件解码
        HARDWARE_ONLY,      // 仅使用硬件解码
        SOFTWARE_ONLY,      // 仅使用软件解码
        AUTO_SELECT         // 自动选择最佳解码器
    };

    /**
     * 创建视频解码器
     * @param strategy 创建策略
     * @param preferred_hw_type 首选的硬件解码器类型
     * @return 解码器智能指针
     */
    static std::unique_ptr<VideoDecoderBase> createDecoder(
        CreationStrategy strategy = CreationStrategy::PREFER_HARDWARE,
        HardwareDecoderType preferred_hw_type = HardwareDecoderType::CUDA
    );

    /**
     * 创建软件解码器
     * @return 软件解码器智能指针
     */
    static std::unique_ptr<SoftwareVideoDecoder> createSoftwareDecoder();

    /**
     * 创建硬件解码器
     * @param hw_type 硬件解码器类型
     * @return 硬件解码器智能指针，如果硬件不支持则返回nullptr
     */
    static std::unique_ptr<HardwareVideoDecoder> createHardwareDecoder(
        HardwareDecoderType hw_type
    );

    /**
     * 检测并创建最佳解码器
     * @param file_path 要解码的视频文件路径
     * @return 最适合的解码器
     */
    static std::unique_ptr<VideoDecoderBase> createBestDecoder(
        const std::string& file_path
    );

    /**
     * 获取推荐的解码器类型
     * @param codec_id 视频编解码器ID
     * @return 推荐的解码器类型列表（按优先级排序）
     */
    static std::vector<DecoderType> getRecommendedDecoders(AVCodecID codec_id);

    /**
     * 检查特定解码器类型是否可用
     * @param decoder_type 解码器类型
     * @return 是否可用
     */
    static bool isDecoderAvailable(DecoderType decoder_type);

    /**
     * 获取系统中所有可用的解码器
     * @return 可用解码器列表
     */
    static std::vector<DecoderType> getAvailableDecoders();

    /**
     * 获取解码器性能信息
     * @param decoder_type 解码器类型
     * @return 性能描述字符串
     */
    static std::string getDecoderPerformanceInfo(DecoderType decoder_type);

private:
    // 静态工具方法
    static bool isCodecSuitableForHardware(AVCodecID codec_id);
    static HardwareDecoderType selectBestHardwareDecoder(AVCodecID codec_id);
    static std::string getCodecName(AVCodecID codec_id);
};

/**
 * 解码器管理器
 * 提供更高级的解码器管理功能
 */
class VideoDecoderManager {
public:
    VideoDecoderManager();
    ~VideoDecoderManager();

    /**
     * 设置全局解码器偏好
     * @param strategy 全局策略
     */
    void setGlobalStrategy(VideoDecoderFactory::CreationStrategy strategy);

    /**
     * 打开视频文件并自动选择最佳解码器
     * @param file_path 视频文件路径
     * @return 是否成功
     */
    bool openVideo(const std::string& file_path);

    /**
     * 获取当前解码器
     * @return 解码器指针
     */
    VideoDecoderBase* getCurrentDecoder() const { return current_decoder_.get(); }

    /**
     * 切换解码器类型
     * @param decoder_type 新的解码器类型
     * @return 是否成功切换
     */
    bool switchDecoder(DecoderType decoder_type);

    /**
     * 获取当前解码器的统计信息
     * @return 统计信息
     */
    DecodingStats getCurrentStats() const;

    /**
     * 关闭当前视频
     */
    void close();

private:
    std::unique_ptr<VideoDecoderBase> current_decoder_;
    VideoDecoderFactory::CreationStrategy global_strategy_;
    std::string current_file_path_;
    bool is_open_;
};

} // namespace StreamKit 