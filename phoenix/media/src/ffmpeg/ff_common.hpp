/**
 * @file ff_common.hpp
 * @brief Common FFmpeg includes and utilities
 * 
 * This file provides common FFmpeg headers and helper functions
 * used across the media module. It isolates FFmpeg dependencies
 * to the src/ directory.
 */

#pragma once

// FFmpeg C headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/codec_types.hpp>
#include <string>

namespace phoenix::media::ff {

/**
 * @brief Convert FFmpeg error code to string
 */
inline std::string avErrorString(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, buf, sizeof(buf));
    return buf;
}

/**
 * @brief Convert FFmpeg error to phoenix Error
 */
inline Error avError(int errnum, const std::string& context = "") {
    std::string msg = context;
    if (!msg.empty()) msg += ": ";
    msg += avErrorString(errnum);
    
    ErrorCode code = ErrorCode::Unknown;
    if (errnum == AVERROR(ENOMEM)) {
        code = ErrorCode::OutOfMemory;
    } else if (errnum == AVERROR(ENOENT) || errnum == AVERROR_STREAM_NOT_FOUND) {
        code = ErrorCode::FileNotFound;
    } else if (errnum == AVERROR_EOF) {
        code = ErrorCode::EndOfFile;
    } else if (errnum == AVERROR_DECODER_NOT_FOUND) {
        code = ErrorCode::CodecNotFound;
    } else if (errnum == AVERROR_INVALIDDATA) {
        code = ErrorCode::InvalidData;
    }
    
    return Error(code, msg);
}

/**
 * @brief Convert AVPixelFormat to phoenix PixelFormat
 */
inline PixelFormat fromAVPixelFormat(AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
        case AV_PIX_FMT_YUV422P: return PixelFormat::YUV422P;
        case AV_PIX_FMT_YUV444P: return PixelFormat::YUV444P;
        case AV_PIX_FMT_NV12: return PixelFormat::NV12;
        case AV_PIX_FMT_NV21: return PixelFormat::NV21;
        case AV_PIX_FMT_RGB24: return PixelFormat::RGB24;
        case AV_PIX_FMT_RGBA: return PixelFormat::RGBA;
        case AV_PIX_FMT_BGRA: return PixelFormat::BGRA;
#ifdef _WIN32
        case AV_PIX_FMT_D3D11: return PixelFormat::D3D11;
#endif
        case AV_PIX_FMT_CUDA: return PixelFormat::CUDA;
        case AV_PIX_FMT_VAAPI: return PixelFormat::VAAPI;
#ifdef __APPLE__
        case AV_PIX_FMT_VIDEOTOOLBOX: return PixelFormat::VideoToolbox;
#endif
        case AV_PIX_FMT_QSV: return PixelFormat::QSV;
        default: return PixelFormat::Unknown;
    }
}

/**
 * @brief Convert phoenix PixelFormat to AVPixelFormat
 */
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
        default: return AV_PIX_FMT_NONE;
    }
}

/**
 * @brief Convert AVSampleFormat to phoenix SampleFormat
 */
inline SampleFormat fromAVSampleFormat(AVSampleFormat fmt) {
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

/**
 * @brief Convert phoenix SampleFormat to AVSampleFormat
 */
inline AVSampleFormat toAVSampleFormat(SampleFormat fmt) {
    switch (fmt) {
        case SampleFormat::U8: return AV_SAMPLE_FMT_U8;
        case SampleFormat::S16: return AV_SAMPLE_FMT_S16;
        case SampleFormat::S32: return AV_SAMPLE_FMT_S32;
        case SampleFormat::Float: return AV_SAMPLE_FMT_FLT;
        case SampleFormat::Double: return AV_SAMPLE_FMT_DBL;
        case SampleFormat::U8P: return AV_SAMPLE_FMT_U8P;
        case SampleFormat::S16P: return AV_SAMPLE_FMT_S16P;
        case SampleFormat::S32P: return AV_SAMPLE_FMT_S32P;
        case SampleFormat::FloatP: return AV_SAMPLE_FMT_FLTP;
        case SampleFormat::DoubleP: return AV_SAMPLE_FMT_DBLP;
        default: return AV_SAMPLE_FMT_NONE;
    }
}

/**
 * @brief Check if pixel format is hardware
 */
inline bool isHardwarePixelFormat(AVPixelFormat fmt) {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(fmt);
    return desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

/**
 * @brief Convert AVRational to phoenix Rational
 */
inline Rational fromAVRational(AVRational r) {
    return {r.num, r.den};
}

/**
 * @brief Convert timestamp from AVStream time_base to microseconds
 */
inline Timestamp toMicroseconds(int64_t pts, AVRational timeBase) {
    if (pts == AV_NOPTS_VALUE) return kNoTimestamp;
    return av_rescale_q(pts, timeBase, {1, 1000000});
}

/**
 * @brief Convert microseconds to AVStream time_base
 */
inline int64_t fromMicroseconds(Timestamp us, AVRational timeBase) {
    if (us == kNoTimestamp) return AV_NOPTS_VALUE;
    return av_rescale_q(us, {1, 1000000}, timeBase);
}

} // namespace phoenix::media::ff
