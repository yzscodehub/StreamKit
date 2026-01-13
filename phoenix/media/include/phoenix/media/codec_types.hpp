/**
 * @file codec_types.hpp
 * @brief Codec-related type definitions
 * 
 * Defines hardware acceleration types, codec preferences,
 * and related enumerations for the media layer.
 */

#pragma once

#include <string>

namespace phoenix::media {

/**
 * @brief Hardware acceleration type
 */
enum class HWAccelType {
    None = 0,       // Software decoding/encoding
    D3D11VA,        // Windows Direct3D 11 Video Acceleration
    CUDA,           // NVIDIA CUDA
    QSV,            // Intel Quick Sync Video
    VAAPI,          // Linux VA-API
    VideoToolbox,   // macOS VideoToolbox
    DXVA2,          // Windows DXVA2 (legacy)
};

/**
 * @brief Codec preference for decoder/encoder selection
 */
enum class CodecPreference {
    Auto,           // Automatically select best available
    PreferHW,       // Prefer hardware, fallback to software
    ForceSW,        // Force software only
    ForceHW,        // Force hardware only (fail if unavailable)
};

/**
 * @brief Decode mode
 */
enum class DecodeMode {
    Sequential,     // Sequential frame access (streaming)
    RandomAccess,   // Random access seeking (editing)
};

/**
 * @brief Get human-readable name for HWAccelType
 */
inline const char* hwAccelTypeName(HWAccelType type) {
    switch (type) {
        case HWAccelType::None: return "Software";
        case HWAccelType::D3D11VA: return "D3D11VA";
        case HWAccelType::CUDA: return "NVIDIA CUDA";
        case HWAccelType::QSV: return "Intel QSV";
        case HWAccelType::VAAPI: return "VA-API";
        case HWAccelType::VideoToolbox: return "VideoToolbox";
        case HWAccelType::DXVA2: return "DXVA2";
        default: return "Unknown";
    }
}

/**
 * @brief Get FFmpeg hwaccel name for HWAccelType
 */
inline const char* hwAccelFFmpegName(HWAccelType type) {
    switch (type) {
        case HWAccelType::D3D11VA: return "d3d11va";
        case HWAccelType::CUDA: return "cuda";
        case HWAccelType::QSV: return "qsv";
        case HWAccelType::VAAPI: return "vaapi";
        case HWAccelType::VideoToolbox: return "videotoolbox";
        case HWAccelType::DXVA2: return "dxva2";
        default: return nullptr;
    }
}

} // namespace phoenix::media
