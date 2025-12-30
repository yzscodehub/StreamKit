#include "video/decoder_factory.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace StreamKit {

// VideoDecoderFactory 实现
std::unique_ptr<VideoDecoderBase> VideoDecoderFactory::createDecoder(
    CreationStrategy strategy,
    HardwareDecoderType preferred_hw_type) {
    
    LOG_INFO("Creating decoder with strategy: {}, preferred HW type: {}", 
             static_cast<int>(strategy), static_cast<int>(preferred_hw_type));
    
    switch (strategy) {
        case CreationStrategy::PREFER_HARDWARE: {
            // 首先尝试硬件解码器
            auto hw_decoder = createHardwareDecoder(preferred_hw_type);
            if (hw_decoder) {
                LOG_INFO("Successfully created preferred hardware decoder");
                return std::unique_ptr<VideoDecoderBase>(hw_decoder.release());
            }
            
            // 硬件解码器失败，回退到软件解码器
            LOG_WARN("Hardware decoder creation failed, falling back to software decoder");
            auto sw_decoder = createSoftwareDecoder();
            if (sw_decoder) {
                LOG_INFO("Successfully created fallback software decoder");
                return std::unique_ptr<VideoDecoderBase>(sw_decoder.release());
            }
            
            LOG_ERROR("Both hardware and software decoder creation failed");
            return nullptr;
        }
        
        case CreationStrategy::PREFER_SOFTWARE: {
            // 首先尝试软件解码器
            auto sw_decoder = createSoftwareDecoder();
            if (sw_decoder) {
                LOG_INFO("Successfully created preferred software decoder");
                return std::unique_ptr<VideoDecoderBase>(sw_decoder.release());
            }
            
            // 软件解码器失败，尝试硬件解码器
            LOG_WARN("Software decoder creation failed, trying hardware decoder");
            auto hw_decoder = createHardwareDecoder(preferred_hw_type);
            if (hw_decoder) {
                LOG_INFO("Successfully created fallback hardware decoder");
                return std::unique_ptr<VideoDecoderBase>(hw_decoder.release());
            }
            
            LOG_ERROR("Both software and hardware decoder creation failed");
            return nullptr;
        }
        
        case CreationStrategy::HARDWARE_ONLY: {
            auto hw_decoder = createHardwareDecoder(preferred_hw_type);
            if (hw_decoder) {
                LOG_INFO("Successfully created hardware-only decoder");
                return std::unique_ptr<VideoDecoderBase>(hw_decoder.release());
            }
            
            LOG_ERROR("Hardware-only decoder creation failed");
            return nullptr;
        }
        
        case CreationStrategy::SOFTWARE_ONLY: {
            auto sw_decoder = createSoftwareDecoder();
            if (sw_decoder) {
                LOG_INFO("Successfully created software-only decoder");
                return std::unique_ptr<VideoDecoderBase>(sw_decoder.release());
            }
            
            LOG_ERROR("Software-only decoder creation failed");
            return nullptr;
        }
        
        case CreationStrategy::AUTO_SELECT: {
            return createBestDecoder("");
        }
        
        default:
            LOG_ERROR("Unknown creation strategy: {}", static_cast<int>(strategy));
            return nullptr;
    }
}

std::unique_ptr<SoftwareVideoDecoder> VideoDecoderFactory::createSoftwareDecoder() {
    LOG_INFO("Creating software video decoder");
    
    try {
        auto decoder = std::make_unique<SoftwareVideoDecoder>();
        LOG_INFO("Software video decoder created successfully");
        return decoder;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create software decoder: {}", e.what());
        return nullptr;
    }
}

std::unique_ptr<HardwareVideoDecoder> VideoDecoderFactory::createHardwareDecoder(
    HardwareDecoderType hw_type) {
    
    LOG_INFO("Creating hardware video decoder: {}", static_cast<int>(hw_type));
    
    // 检查硬件类型是否支持
    if (!HardwareVideoDecoder::isHardwareTypeSupported(hw_type)) {
        LOG_WARN("Hardware decoder type {} is not supported on this system", 
                 static_cast<int>(hw_type));
        return nullptr;
    }
    
    try {
        auto decoder = std::make_unique<HardwareVideoDecoder>(hw_type);
        LOG_INFO("Hardware video decoder created successfully");
        return decoder;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create hardware decoder: {}", e.what());
        return nullptr;
    }
}

std::unique_ptr<VideoDecoderBase> VideoDecoderFactory::createBestDecoder(
    const std::string& file_path) {
    
    LOG_INFO("Auto-selecting best decoder for file: {}", file_path.empty() ? "unknown" : file_path);
    
    // 获取可用的硬件解码器
    auto hw_decoders = VideoDecoderBase::getAvailableHardwareDecoders();
    
    // 按优先级排序硬件解码器
    std::vector<HardwareDecoderType> hw_priority = {
        HardwareDecoderType::CUDA,      // NVIDIA GPU (highest performance)
        HardwareDecoderType::D3D11VA,   // Modern Windows HW acceleration
        HardwareDecoderType::DXVA2,     // Legacy Windows HW acceleration
        HardwareDecoderType::QSV,       // Intel Quick Sync
        HardwareDecoderType::VAAPI      // Linux Intel/AMD
    };
    
    // 尝试创建可用的硬件解码器
    for (HardwareDecoderType hw_type : hw_priority) {
        auto it = std::find_if(hw_decoders.begin(), hw_decoders.end(),
            [hw_type](const HardwareDecoderInfo& info) {
                return info.type == hw_type && info.available;
            });
        
        if (it != hw_decoders.end()) {
            auto decoder = createHardwareDecoder(hw_type);
            if (decoder) {
                LOG_INFO("Auto-selected hardware decoder: {}", it->name);
                return std::unique_ptr<VideoDecoderBase>(decoder.release());
            }
        }
    }
    
    // 如果没有可用的硬件解码器，使用软件解码器
    auto sw_decoder = createSoftwareDecoder();
    if (sw_decoder) {
        LOG_INFO("Auto-selected software decoder (no hardware acceleration available)");
        return std::unique_ptr<VideoDecoderBase>(sw_decoder.release());
    }
    
    LOG_ERROR("Failed to create any decoder (both hardware and software failed)");
    return nullptr;
}

std::vector<DecoderType> VideoDecoderFactory::getRecommendedDecoders(AVCodecID codec_id) {
    std::vector<DecoderType> recommendations;
    
    // 根据编解码器类型推荐解码器
    switch (codec_id) {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_HEVC:
            // H.264/H.265 有很好的硬件支持
            recommendations = {
                DecoderType::HARDWARE_CUDA,
                DecoderType::HARDWARE_D3D11VA,
                DecoderType::HARDWARE_DXVA2,
                DecoderType::HARDWARE_QSV,
                DecoderType::SOFTWARE
            };
            break;
            
        case AV_CODEC_ID_VP9:
        case AV_CODEC_ID_AV1:
            // VP9/AV1 主要由现代硬件支持
            recommendations = {
                DecoderType::HARDWARE_CUDA,
                DecoderType::SOFTWARE
            };
            break;
            
        case AV_CODEC_ID_MPEG2VIDEO:
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_WMV3:
            // 较老的编解码器，部分硬件支持
            recommendations = {
                DecoderType::HARDWARE_DXVA2,
                DecoderType::HARDWARE_CUDA,
                DecoderType::SOFTWARE
            };
            break;
            
        default:
            // 其他编解码器优先使用软件解码
            recommendations = {
                DecoderType::SOFTWARE,
                DecoderType::HARDWARE_CUDA
            };
            break;
    }
    
    // 过滤掉不可用的解码器
    std::vector<DecoderType> filtered_recommendations;
    for (DecoderType type : recommendations) {
        if (isDecoderAvailable(type)) {
            filtered_recommendations.push_back(type);
        }
    }
    
    return filtered_recommendations;
}

bool VideoDecoderFactory::isDecoderAvailable(DecoderType decoder_type) {
    switch (decoder_type) {
        case DecoderType::SOFTWARE:
            // 软件解码器总是可用的
            return true;
            
        case DecoderType::HARDWARE_CUDA:
            return HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType::CUDA);
            
        case DecoderType::HARDWARE_DXVA2:
            return HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType::DXVA2);
            
        case DecoderType::HARDWARE_D3D11VA:
            return HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType::D3D11VA);
            
        case DecoderType::HARDWARE_VAAPI:
            return HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType::VAAPI);
            
        case DecoderType::HARDWARE_QSV:
            return HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType::QSV);
            
        case DecoderType::HARDWARE_VIDEOTOOLBOX:
            return HardwareVideoDecoder::isHardwareTypeSupported(HardwareDecoderType::VIDEOTOOLBOX);
            
        default:
            return false;
    }
}

std::vector<DecoderType> VideoDecoderFactory::getAvailableDecoders() {
    std::vector<DecoderType> available_decoders;
    
    // 检查所有解码器类型
    std::vector<DecoderType> all_types = {
        DecoderType::SOFTWARE,
        DecoderType::HARDWARE_CUDA,
        DecoderType::HARDWARE_DXVA2,
        DecoderType::HARDWARE_D3D11VA,
        DecoderType::HARDWARE_VAAPI,
        DecoderType::HARDWARE_QSV,
        DecoderType::HARDWARE_VIDEOTOOLBOX
    };
    
    for (DecoderType type : all_types) {
        if (isDecoderAvailable(type)) {
            available_decoders.push_back(type);
        }
    }
    
    return available_decoders;
}

std::string VideoDecoderFactory::getDecoderPerformanceInfo(DecoderType decoder_type) {
    switch (decoder_type) {
        case DecoderType::SOFTWARE:
            return "CPU解码，兼容性最好，性能中等，支持所有格式";
            
        case DecoderType::HARDWARE_CUDA:
            return "NVIDIA GPU加速，性能最高，支持H.264/H.265/VP9/AV1";
            
        case DecoderType::HARDWARE_DXVA2:
            return "Windows硬件加速(Legacy)，兼容性好，性能较高";
            
        case DecoderType::HARDWARE_D3D11VA:
            return "Windows硬件加速(Modern)，性能高，支持现代GPU";
            
        case DecoderType::HARDWARE_VAAPI:
            return "Linux Intel/AMD硬件加速，中等性能";
            
        case DecoderType::HARDWARE_QSV:
            return "Intel Quick Sync加速，低功耗，中等性能";
            
        case DecoderType::HARDWARE_VIDEOTOOLBOX:
            return "Apple硬件加速，Mac平台专用，性能高";
            
        default:
            return "未知解码器类型";
    }
}

// 私有静态方法实现
bool VideoDecoderFactory::isCodecSuitableForHardware(AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_HEVC:
        case AV_CODEC_ID_VP9:
        case AV_CODEC_ID_AV1:
        case AV_CODEC_ID_MPEG2VIDEO:
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_WMV3:
            return true;
        default:
            return false;
    }
}

HardwareDecoderType VideoDecoderFactory::selectBestHardwareDecoder(AVCodecID codec_id) {
    auto hw_decoders = VideoDecoderBase::getAvailableHardwareDecoders();
    
    // 为不同编解码器选择最佳硬件解码器
    std::vector<HardwareDecoderType> preferred_order;
    
    switch (codec_id) {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_HEVC:
            preferred_order = {
                HardwareDecoderType::CUDA,
                HardwareDecoderType::D3D11VA,
                HardwareDecoderType::QSV,
                HardwareDecoderType::DXVA2
            };
            break;
            
        case AV_CODEC_ID_VP9:
        case AV_CODEC_ID_AV1:
            preferred_order = {
                HardwareDecoderType::CUDA
            };
            break;
            
        default:
            preferred_order = {
                HardwareDecoderType::CUDA,
                HardwareDecoderType::D3D11VA,
                HardwareDecoderType::DXVA2
            };
            break;
    }
    
    // 返回第一个可用的硬件解码器
    for (HardwareDecoderType hw_type : preferred_order) {
        auto it = std::find_if(hw_decoders.begin(), hw_decoders.end(),
            [hw_type](const HardwareDecoderInfo& info) {
                return info.type == hw_type && info.available;
            });
        
        if (it != hw_decoders.end()) {
            return hw_type;
        }
    }
    
    return HardwareDecoderType::NONE;
}

std::string VideoDecoderFactory::getCodecName(AVCodecID codec_id) {
    const AVCodecDescriptor* desc = avcodec_descriptor_get(codec_id);
    return desc ? desc->name : "unknown";
}

// VideoDecoderManager 实现
VideoDecoderManager::VideoDecoderManager()
    : current_decoder_(nullptr)
    , global_strategy_(VideoDecoderFactory::CreationStrategy::PREFER_HARDWARE)
    , is_open_(false) {
}

VideoDecoderManager::~VideoDecoderManager() {
    close();
}

void VideoDecoderManager::setGlobalStrategy(VideoDecoderFactory::CreationStrategy strategy) {
    global_strategy_ = strategy;
    LOG_INFO("Global decoder strategy set to: {}", static_cast<int>(strategy));
}

bool VideoDecoderManager::openVideo(const std::string& file_path) {
    if (is_open_) {
        LOG_WARN("Video already open, closing previous video");
        close();
    }
    
    current_file_path_ = file_path;
    
    // 使用全局策略创建解码器
    current_decoder_ = VideoDecoderFactory::createDecoder(global_strategy_);
    if (!current_decoder_) {
        LOG_ERROR("Failed to create decoder for file: {}", file_path);
        return false;
    }
    
    // 初始化解码器
    if (!current_decoder_->initialize(file_path)) {
        LOG_ERROR("Failed to initialize decoder for file: {}", file_path);
        current_decoder_.reset();
        return false;
    }
    
    is_open_ = true;
    LOG_INFO("Video opened successfully with decoder: {}", current_decoder_->getDecoderName());
    
    return true;
}

bool VideoDecoderManager::switchDecoder(DecoderType decoder_type) {
    if (!is_open_ || current_file_path_.empty()) {
        LOG_ERROR("No video is currently open");
        return false;
    }
    
    LOG_INFO("Switching to decoder type: {}", static_cast<int>(decoder_type));
    
    // 创建新的解码器
    std::unique_ptr<VideoDecoderBase> new_decoder;
    
    switch (decoder_type) {
        case DecoderType::SOFTWARE:
            new_decoder = VideoDecoderFactory::createSoftwareDecoder();
            break;
            
        case DecoderType::HARDWARE_CUDA:
            new_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::CUDA);
            break;
            
        case DecoderType::HARDWARE_DXVA2:
            new_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::DXVA2);
            break;
            
        case DecoderType::HARDWARE_D3D11VA:
            new_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::D3D11VA);
            break;
            
        case DecoderType::HARDWARE_QSV:
            new_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::QSV);
            break;
            
        case DecoderType::HARDWARE_VAAPI:
            new_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::VAAPI);
            break;
            
        default:
            LOG_ERROR("Unsupported decoder type: {}", static_cast<int>(decoder_type));
            return false;
    }
    
    if (!new_decoder) {
        LOG_ERROR("Failed to create new decoder");
        return false;
    }
    
    // 初始化新解码器
    if (!new_decoder->initialize(current_file_path_)) {
        LOG_ERROR("Failed to initialize new decoder");
        return false;
    }
    
    // 保存当前位置
    int current_position = current_decoder_->getCurrentFrameNumber();
    
    // 替换解码器
    current_decoder_ = std::move(new_decoder);
    
    // 尝试恢复位置
    if (current_position >= 0) {
        current_decoder_->seekToFrame(current_position);
    }
    
    LOG_INFO("Successfully switched to decoder: {}", current_decoder_->getDecoderName());
    return true;
}

DecodingStats VideoDecoderManager::getCurrentStats() const {
    if (current_decoder_) {
        return current_decoder_->getStats();
    }
    
    // 返回空的统计信息
    DecodingStats empty_stats = {};
    return empty_stats;
}

void VideoDecoderManager::close() {
    if (current_decoder_) {
        current_decoder_->close();
        current_decoder_.reset();
    }
    
    is_open_ = false;
    current_file_path_.clear();
    
    LOG_INFO("Video decoder manager closed");
}

} // namespace StreamKit 