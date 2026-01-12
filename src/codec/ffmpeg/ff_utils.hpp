/**
 * @file ff_utils.hpp
 * @brief RAII wrappers and utilities for FFmpeg types
 * 
 * Provides safe, modern C++ wrappers for FFmpeg C types:
 * - PacketWrapper: RAII AVPacket
 * - FormatContextPtr: unique_ptr for AVFormatContext
 * - CodecContextPtr: unique_ptr for AVCodecContext
 * - SwsContextPtr: unique_ptr for SwsContext
 * - SwrContextPtr: unique_ptr for SwrContext
 */

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include <string>
#include <cstdint>

#include "core/types.hpp"
#include "core/result.hpp"

namespace phoenix {

// ============================================================================
// Error Conversion
// ============================================================================

/// Convert FFmpeg error code to string
inline std::string ffmpegErrorString(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

/// Convert FFmpeg error to phoenix Error
inline Error ffmpegError(int errnum, const std::string& context = "") {
    std::string msg = context;
    if (!msg.empty()) {
        msg += ": ";
    }
    msg += ffmpegErrorString(errnum);
    
    ErrorCode code = ErrorCode::Unknown;
    if (errnum == AVERROR_EOF) {
        code = ErrorCode::EndOfFile;
    } else if (errnum == AVERROR(ENOMEM)) {
        code = ErrorCode::OutOfMemory;
    } else if (errnum == AVERROR(ENOENT)) {
        code = ErrorCode::FileNotFound;
    } else if (errnum == AVERROR_INVALIDDATA) {
        code = ErrorCode::InvalidData;
    } else if (errnum == AVERROR_DECODER_NOT_FOUND) {
        code = ErrorCode::CodecNotFound;
    }
    
    return Error(code, std::move(msg));
}

// ============================================================================
// Timestamp Conversion
// ============================================================================

/// Convert FFmpeg pts to microseconds using av_rescale_q (handles overflow)
inline Timestamp ptsToMicroseconds(int64_t pts, AVRational timeBase) {
    if (pts == AV_NOPTS_VALUE) {
        return kNoTimestamp;
    }
    // av_rescale_q handles overflow correctly
    return av_rescale_q(pts, timeBase, AVRational{1, 1000000});
}

/// Convert microseconds to FFmpeg pts
inline int64_t microsecondsToPts(Timestamp us, AVRational timeBase) {
    if (us == kNoTimestamp) {
        return AV_NOPTS_VALUE;
    }
    return av_rescale_q(us, AVRational{1, 1000000}, timeBase);
}

/// Get duration in microseconds from FFmpeg duration
inline Duration durationToMicroseconds(int64_t duration, AVRational timeBase) {
    if (duration == AV_NOPTS_VALUE || duration <= 0) {
        return 0;
    }
    return av_rescale_q(duration, timeBase, AVRational{1, 1000000});
}

// ============================================================================
// PacketWrapper - RAII AVPacket
// ============================================================================

/**
 * @brief Move-only RAII wrapper for AVPacket
 * 
 * Unlike SharedAVFrame, packets don't need shared ownership.
 * Each packet is consumed by one decoder.
 */
class PacketWrapper {
public:
    /// Default constructor: allocates empty packet
    PacketWrapper() : pkt_(av_packet_alloc()) {}
    
    /// Destructor: frees packet
    ~PacketWrapper() {
        if (pkt_) {
            av_packet_free(&pkt_);
        }
    }
    
    // ========== Move Only ==========
    
    PacketWrapper(PacketWrapper&& other) noexcept : pkt_(other.pkt_) {
        other.pkt_ = nullptr;
    }
    
    PacketWrapper& operator=(PacketWrapper&& other) noexcept {
        if (this != &other) {
            if (pkt_) {
                av_packet_free(&pkt_);
            }
            pkt_ = other.pkt_;
            other.pkt_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy operations
    PacketWrapper(const PacketWrapper&) = delete;
    PacketWrapper& operator=(const PacketWrapper&) = delete;
    
    // ========== Accessors ==========
    
    [[nodiscard]] AVPacket* get() const { return pkt_; }
    AVPacket* operator->() const { return pkt_; }
    AVPacket& operator*() const { return *pkt_; }
    [[nodiscard]] explicit operator bool() const { return pkt_ != nullptr; }
    
    // ========== Operations ==========
    
    /// Unreference packet data (keep wrapper for reuse)
    void unref() {
        if (pkt_) {
            av_packet_unref(pkt_);
        }
    }
    
    /// Check if packet is a keyframe
    [[nodiscard]] bool isKeyframe() const {
        return pkt_ && (pkt_->flags & AV_PKT_FLAG_KEY);
    }
    
    /// Get packet size
    [[nodiscard]] int size() const {
        return pkt_ ? pkt_->size : 0;
    }
    
    /// Get packet data pointer
    [[nodiscard]] uint8_t* data() const {
        return pkt_ ? pkt_->data : nullptr;
    }
    
private:
    AVPacket* pkt_ = nullptr;
};

// ============================================================================
// Smart Pointer Deleters
// ============================================================================

/// Deleter for AVFormatContext
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            avformat_close_input(&ctx);
        }
    }
};

/// Deleter for AVFormatContext (output)
struct AVFormatContextOutputDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            if (ctx->pb) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        }
    }
};

/// Deleter for AVCodecContext
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};

/// Deleter for SwsContext
struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const {
        if (ctx) {
            sws_freeContext(ctx);
        }
    }
};

/// Deleter for SwrContext
struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const {
        if (ctx) {
            swr_free(&ctx);
        }
    }
};

/// Deleter for AVBufferRef (hardware context)
struct AVBufferRefDeleter {
    void operator()(AVBufferRef* ref) const {
        if (ref) {
            av_buffer_unref(&ref);
        }
    }
};

// ============================================================================
// Smart Pointer Type Aliases
// ============================================================================

using FormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using FormatContextOutputPtr = std::unique_ptr<AVFormatContext, AVFormatContextOutputDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using HWDeviceContextPtr = std::unique_ptr<AVBufferRef, AVBufferRefDeleter>;

// ============================================================================
// Factory Functions
// ============================================================================

/// Open input file and create format context
inline Result<FormatContextPtr> openInputFile(const std::string& filename) {
    AVFormatContext* ctx = nullptr;
    int ret = avformat_open_input(&ctx, filename.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return Err<FormatContextPtr>(ffmpegError(ret, "avformat_open_input"));
    }
    
    ret = avformat_find_stream_info(ctx, nullptr);
    if (ret < 0) {
        avformat_close_input(&ctx);
        return Err<FormatContextPtr>(ffmpegError(ret, "avformat_find_stream_info"));
    }
    
    return FormatContextPtr(ctx);
}

/// Create decoder context for a stream
inline Result<CodecContextPtr> createDecoderContext(AVStream* stream) {
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        return Err<CodecContextPtr>(ErrorCode::CodecNotFound, 
            "Decoder not found for codec: " + std::to_string(stream->codecpar->codec_id));
    }
    
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        return Err<CodecContextPtr>(ErrorCode::OutOfMemory, "Failed to allocate codec context");
    }
    
    CodecContextPtr ctxPtr(ctx);
    
    int ret = avcodec_parameters_to_context(ctx, stream->codecpar);
    if (ret < 0) {
        return Err<CodecContextPtr>(ffmpegError(ret, "avcodec_parameters_to_context"));
    }
    
    // Set threading mode
    ctx->thread_count = 0;  // Auto-detect
    ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    
    ret = avcodec_open2(ctx, codec, nullptr);
    if (ret < 0) {
        return Err<CodecContextPtr>(ffmpegError(ret, "avcodec_open2"));
    }
    
    return ctxPtr;
}

/// Find best stream of given type
inline int findBestStream(AVFormatContext* ctx, AVMediaType type) {
    return av_find_best_stream(ctx, type, -1, -1, nullptr, 0);
}

// ============================================================================
// PixelFormat Conversion
// ============================================================================

/// Convert AVPixelFormat to phoenix PixelFormat
inline PixelFormat toPixelFormat(AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
        case AV_PIX_FMT_YUV422P: return PixelFormat::YUV422P;
        case AV_PIX_FMT_YUV444P: return PixelFormat::YUV444P;
        case AV_PIX_FMT_NV12: return PixelFormat::NV12;
        case AV_PIX_FMT_NV21: return PixelFormat::NV21;
        case AV_PIX_FMT_RGB24: return PixelFormat::RGB24;
        case AV_PIX_FMT_RGBA: return PixelFormat::RGBA;
        case AV_PIX_FMT_BGRA: return PixelFormat::BGRA;
        case AV_PIX_FMT_D3D11: return PixelFormat::D3D11;
        case AV_PIX_FMT_CUDA: return PixelFormat::CUDA;
        case AV_PIX_FMT_VAAPI: return PixelFormat::VAAPI;
        case AV_PIX_FMT_VIDEOTOOLBOX: return PixelFormat::VideoToolbox;
        default: return PixelFormat::Unknown;
    }
}

/// Convert phoenix PixelFormat to AVPixelFormat
inline AVPixelFormat toAVPixelFormat(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::YUV420P: return AV_PIX_FMT_YUV420P;
        case PixelFormat::YUV422P: return AV_PIX_FMT_YUV422P;
        case PixelFormat::YUV444P: return AV_PIX_FMT_YUV444P;
        case PixelFormat::NV12: return AV_PIX_FMT_NV12;
        case PixelFormat::NV21: return AV_PIX_FMT_NV21;
        case PixelFormat::RGB24: return AV_PIX_FMT_RGB24;
        case PixelFormat::RGBA: return AV_PIX_FMT_RGBA;
        case PixelFormat::BGRA: return AV_PIX_FMT_BGRA;
        case PixelFormat::D3D11: return AV_PIX_FMT_D3D11;
        case PixelFormat::CUDA: return AV_PIX_FMT_CUDA;
        case PixelFormat::VAAPI: return AV_PIX_FMT_VAAPI;
        case PixelFormat::VideoToolbox: return AV_PIX_FMT_VIDEOTOOLBOX;
        default: return AV_PIX_FMT_NONE;
    }
}

/// Convert AVSampleFormat to phoenix SampleFormat
inline SampleFormat toSampleFormat(AVSampleFormat fmt) {
    switch (fmt) {
        case AV_SAMPLE_FMT_U8: return SampleFormat::U8;
        case AV_SAMPLE_FMT_S16: return SampleFormat::S16;
        case AV_SAMPLE_FMT_S32: return SampleFormat::S32;
        case AV_SAMPLE_FMT_FLT: return SampleFormat::Float;
        case AV_SAMPLE_FMT_DBL: return SampleFormat::Double;
        case AV_SAMPLE_FMT_U8P: return SampleFormat::U8P;
        case AV_SAMPLE_FMT_S16P: return SampleFormat::S16P;
        case AV_SAMPLE_FMT_S32P: return SampleFormat::S32P;
        case AV_SAMPLE_FMT_FLTP: return SampleFormat::FloatP;
        case AV_SAMPLE_FMT_DBLP: return SampleFormat::DoubleP;
        default: return SampleFormat::Unknown;
    }
}

} // namespace phoenix

