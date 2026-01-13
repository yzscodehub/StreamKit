/**
 * @file decoder.cpp
 * @brief Decoder implementation
 */

#include <phoenix/media/decoder.hpp>
#include "ffmpeg/ff_common.hpp"
#include "ffmpeg/shared_avframe.hpp"
#include "ffmpeg/frame_impl.hpp"
#include <chrono>

namespace phoenix::media {

// ============================================================================
// Decoder Implementation
// ============================================================================

struct Decoder::Impl {
    DecoderConfig config;
    std::filesystem::path filePath;
    
    // FFmpeg contexts
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* videoCodecCtx = nullptr;
    AVCodecContext* audioCodecCtx = nullptr;
    
    // Stream indices
    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    
    // Hardware acceleration
    AVBufferRef* hwDeviceCtx = nullptr;
    HWAccelType activeHWType = HWAccelType::None;
    
    // Current position tracking
    Timestamp currentVideoPos = 0;
    Timestamp currentAudioPos = 0;
    int64_t currentFrameNumber = 0;
    
    // Decoding state
    ff::SharedAVPacket packet;
    ff::SharedAVFrame decodedFrame;
    bool videoEOF = false;
    bool audioEOF = false;
    
    // Statistics
    DecoderStats stats;
    
    Impl() {
        packet = ff::SharedAVPacket::alloc();
        decodedFrame = ff::SharedAVFrame::alloc();
    }
    
    ~Impl() {
        close();
    }
    
    void close() {
        if (videoCodecCtx) {
            avcodec_free_context(&videoCodecCtx);
        }
        if (audioCodecCtx) {
            avcodec_free_context(&audioCodecCtx);
        }
        if (hwDeviceCtx) {
            av_buffer_unref(&hwDeviceCtx);
        }
        if (formatCtx) {
            avformat_close_input(&formatCtx);
        }
        
        videoStreamIdx = -1;
        audioStreamIdx = -1;
        activeHWType = HWAccelType::None;
        currentVideoPos = 0;
        currentAudioPos = 0;
        currentFrameNumber = 0;
        videoEOF = false;
        audioEOF = false;
    }
    
    Result<void, Error> open() {
        // Open input
        int ret = avformat_open_input(&formatCtx, filePath.string().c_str(),
            nullptr, nullptr);
        if (ret < 0) {
            return ff::avError(ret, "Failed to open file");
        }
        
        // Find stream info
        ret = avformat_find_stream_info(formatCtx, nullptr);
        if (ret < 0) {
            return ff::avError(ret, "Failed to find stream info");
        }
        
        // Open video stream
        videoStreamIdx = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO,
            -1, -1, nullptr, 0);
        if (videoStreamIdx >= 0) {
            auto result = openCodec(AVMEDIA_TYPE_VIDEO);
            if (!result.ok()) {
                // Video open failed, but we might have audio
                videoStreamIdx = -1;
            }
        }
        
        // Open audio stream
        audioStreamIdx = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO,
            -1, -1, nullptr, 0);
        if (audioStreamIdx >= 0) {
            auto result = openCodec(AVMEDIA_TYPE_AUDIO);
            if (!result.ok()) {
                audioStreamIdx = -1;
            }
        }
        
        if (videoStreamIdx < 0 && audioStreamIdx < 0) {
            return Error(ErrorCode::CodecNotFound, "No video or audio streams found");
        }
        
        return Ok();
    }
    
    Result<void, Error> openCodec(AVMediaType type) {
        int streamIdx = (type == AVMEDIA_TYPE_VIDEO) ? videoStreamIdx : audioStreamIdx;
        if (streamIdx < 0) {
            return Error(ErrorCode::NotFound, "Stream not found");
        }
        
        AVStream* stream = formatCtx->streams[streamIdx];
        AVCodecParameters* codecpar = stream->codecpar;
        
        // Find decoder
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            return Error(ErrorCode::CodecNotFound, "Decoder not found");
        }
        
        // Allocate context
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            return Error(ErrorCode::OutOfMemory, "Failed to allocate codec context");
        }
        
        // Copy parameters
        int ret = avcodec_parameters_to_context(codecCtx, codecpar);
        if (ret < 0) {
            avcodec_free_context(&codecCtx);
            return ff::avError(ret, "Failed to copy codec parameters");
        }
        
        // Set threading
        if (config.threadCount > 0) {
            codecCtx->thread_count = config.threadCount;
        } else {
            codecCtx->thread_count = 0;  // Auto
        }
        codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        
        // Try hardware acceleration for video
        if (type == AVMEDIA_TYPE_VIDEO && 
            config.codecPreference != CodecPreference::ForceSW) {
            tryHardwareAccel(codecCtx, codec);
        }
        
        // Open codec
        ret = avcodec_open2(codecCtx, codec, nullptr);
        if (ret < 0) {
            avcodec_free_context(&codecCtx);
            return ff::avError(ret, "Failed to open codec");
        }
        
        // Store context
        if (type == AVMEDIA_TYPE_VIDEO) {
            videoCodecCtx = codecCtx;
        } else {
            audioCodecCtx = codecCtx;
        }
        
        return Ok();
    }
    
    void tryHardwareAccel(AVCodecContext* codecCtx, const AVCodec* codec) {
        // List of hardware types to try (platform-dependent order)
        std::vector<AVHWDeviceType> hwTypes;
        
#ifdef _WIN32
        hwTypes = {AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_CUDA, 
                   AV_HWDEVICE_TYPE_QSV};
#elif defined(__APPLE__)
        hwTypes = {AV_HWDEVICE_TYPE_VIDEOTOOLBOX};
#else
        hwTypes = {AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_CUDA};
#endif
        
        for (AVHWDeviceType hwType : hwTypes) {
            // Check if codec supports this HW type
            for (int i = 0;; i++) {
                const AVCodecHWConfig* hwConfig = avcodec_get_hw_config(codec, i);
                if (!hwConfig) break;
                
                if (hwConfig->device_type == hwType &&
                    (hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                    
                    // Try to create device context
                    int ret = av_hwdevice_ctx_create(&hwDeviceCtx, hwType,
                        nullptr, nullptr, 0);
                    if (ret >= 0) {
                        codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
                        
                        // Map to our enum
                        switch (hwType) {
                            case AV_HWDEVICE_TYPE_D3D11VA:
                                activeHWType = HWAccelType::D3D11VA;
                                break;
                            case AV_HWDEVICE_TYPE_CUDA:
                                activeHWType = HWAccelType::CUDA;
                                break;
                            case AV_HWDEVICE_TYPE_QSV:
                                activeHWType = HWAccelType::QSV;
                                break;
                            case AV_HWDEVICE_TYPE_VAAPI:
                                activeHWType = HWAccelType::VAAPI;
                                break;
#ifdef __APPLE__
                            case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
                                activeHWType = HWAccelType::VideoToolbox;
                                break;
#endif
                            default:
                                break;
                        }
                        return;  // Success
                    }
                }
            }
        }
        
        // No hardware acceleration available
        activeHWType = HWAccelType::None;
    }
    
    Result<VideoFrame, Error> decodeVideo(Timestamp targetTime, bool sequential) {
        if (!videoCodecCtx || videoStreamIdx < 0) {
            return Error(ErrorCode::NotSupported, "No video stream");
        }
        
        AVStream* stream = formatCtx->streams[videoStreamIdx];
        auto startTime = std::chrono::steady_clock::now();
        
        // Seek if needed (for random access)
        if (!sequential && targetTime != kNoTimestamp) {
            // Check if we need to seek
            Duration frameDur = av_rescale_q(1, av_inv_q(stream->avg_frame_rate),
                {1, 1000000});
            
            if (targetTime < currentVideoPos || 
                targetTime > currentVideoPos + frameDur * 10) {
                // Need to seek
                int64_t seekTs = ff::fromMicroseconds(targetTime, stream->time_base);
                int ret = av_seek_frame(formatCtx, videoStreamIdx, seekTs,
                    AVSEEK_FLAG_BACKWARD);
                if (ret < 0) {
                    return ff::avError(ret, "Seek failed");
                }
                avcodec_flush_buffers(videoCodecCtx);
                videoEOF = false;
                ++stats.seekCount;
            }
        }
        
        // Decode loop
        while (!videoEOF) {
            // Try to receive frame
            decodedFrame.unref();
            int ret = avcodec_receive_frame(videoCodecCtx, decodedFrame.get());
            
            if (ret == 0) {
                // Got frame
                Timestamp pts = ff::toMicroseconds(
                    decodedFrame->pts, stream->time_base);
                
                // For random access, skip frames before target
                if (!sequential && targetTime != kNoTimestamp && 
                    pts < targetTime) {
                    // Check if this is close enough (within one frame)
                    Duration frameDur = av_rescale_q(1, 
                        av_inv_q(stream->avg_frame_rate), {1, 1000000});
                    if (targetTime - pts > frameDur) {
                        continue;  // Keep decoding
                    }
                }
                
                currentVideoPos = pts;
                ++stats.framesDecoded;
                ++currentFrameNumber;
                
                // Calculate decode time
                auto endTime = std::chrono::steady_clock::now();
                double decodeMs = std::chrono::duration<double, std::milli>(
                    endTime - startTime).count();
                stats.avgDecodeTimeMs = 
                    (stats.avgDecodeTimeMs * (stats.framesDecoded - 1) + decodeMs) / 
                    stats.framesDecoded;
                
                // Create VideoFrame
                VideoFrame frame;
                frame.m_impl = std::make_shared<VideoFrame::Impl>();
                frame.m_impl->frame = decodedFrame.ref();
                frame.m_impl->frame->pts = pts;  // Store in microseconds
                frame.m_impl->hwType = activeHWType;
                frame.m_impl->frameNumber = currentFrameNumber;
                
                return frame;
            }
            
            if (ret == AVERROR(EAGAIN)) {
                // Need more data - read packet
                packet.unref();
                ret = av_read_frame(formatCtx, packet.get());
                
                if (ret == AVERROR_EOF) {
                    // Flush decoder
                    avcodec_send_packet(videoCodecCtx, nullptr);
                    videoEOF = true;
                    continue;
                }
                
                if (ret < 0) {
                    return ff::avError(ret, "Failed to read frame");
                }
                
                if (packet->stream_index != videoStreamIdx) {
                    continue;  // Wrong stream
                }
                
                ret = avcodec_send_packet(videoCodecCtx, packet.get());
                if (ret < 0 && ret != AVERROR(EAGAIN)) {
                    return ff::avError(ret, "Failed to send packet");
                }
                continue;
            }
            
            if (ret == AVERROR_EOF) {
                return Error(ErrorCode::EndOfFile, "End of video stream");
            }
            
            return ff::avError(ret, "Decode error");
        }
        
        return Error(ErrorCode::EndOfFile, "End of video stream");
    }
    
    Result<AudioFrame, Error> decodeAudio(Timestamp targetTime, bool sequential) {
        if (!audioCodecCtx || audioStreamIdx < 0) {
            return Error(ErrorCode::NotSupported, "No audio stream");
        }
        
        AVStream* stream = formatCtx->streams[audioStreamIdx];
        
        // Seek if needed
        if (!sequential && targetTime != kNoTimestamp) {
            if (targetTime < currentAudioPos - 100000 ||
                targetTime > currentAudioPos + 100000) {
                int64_t seekTs = ff::fromMicroseconds(targetTime, stream->time_base);
                av_seek_frame(formatCtx, audioStreamIdx, seekTs, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(audioCodecCtx);
                audioEOF = false;
            }
        }
        
        while (!audioEOF) {
            decodedFrame.unref();
            int ret = avcodec_receive_frame(audioCodecCtx, decodedFrame.get());
            
            if (ret == 0) {
                Timestamp pts = ff::toMicroseconds(
                    decodedFrame->pts, stream->time_base);
                currentAudioPos = pts;
                
                AudioFrame frame;
                frame.m_impl = std::make_shared<AudioFrame::Impl>();
                frame.m_impl->frame = decodedFrame.ref();
                frame.m_impl->frame->pts = pts;
                
                return frame;
            }
            
            if (ret == AVERROR(EAGAIN)) {
                packet.unref();
                ret = av_read_frame(formatCtx, packet.get());
                
                if (ret == AVERROR_EOF) {
                    avcodec_send_packet(audioCodecCtx, nullptr);
                    audioEOF = true;
                    continue;
                }
                
                if (ret < 0) {
                    return ff::avError(ret, "Failed to read frame");
                }
                
                if (packet->stream_index != audioStreamIdx) {
                    continue;
                }
                
                avcodec_send_packet(audioCodecCtx, packet.get());
                continue;
            }
            
            if (ret == AVERROR_EOF) {
                return Error(ErrorCode::EndOfFile, "End of audio stream");
            }
            
            return ff::avError(ret, "Audio decode error");
        }
        
        return Error(ErrorCode::EndOfFile, "End of audio stream");
    }
    
    Result<void, Error> seekTo(Timestamp time) {
        if (!formatCtx) {
            return Error(ErrorCode::InvalidArgument, "Decoder not open");
        }
        
        int64_t seekTs = av_rescale_q(time, {1, 1000000}, AV_TIME_BASE_Q);
        int ret = av_seek_frame(formatCtx, -1, seekTs, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            return ff::avError(ret, "Seek failed");
        }
        
        if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx);
        if (audioCodecCtx) avcodec_flush_buffers(audioCodecCtx);
        
        currentVideoPos = time;
        currentAudioPos = time;
        videoEOF = false;
        audioEOF = false;
        ++stats.seekCount;
        
        return Ok();
    }
};

// ============================================================================
// Decoder Public Interface
// ============================================================================

Decoder::Decoder() : m_impl(std::make_unique<Impl>()) {}
Decoder::~Decoder() = default;

Decoder::Decoder(Decoder&& other) noexcept = default;
Decoder& Decoder::operator=(Decoder&& other) noexcept = default;

Result<void, Error> Decoder::open(const DecoderConfig& config) {
    close();
    m_impl->config = config;
    m_impl->filePath = config.path;
    return m_impl->open();
}

Result<void, Error> Decoder::open(const std::filesystem::path& path) {
    DecoderConfig config;
    config.path = path;
    return open(config);
}

void Decoder::close() {
    m_impl->close();
}

bool Decoder::isOpen() const {
    return m_impl->formatCtx != nullptr;
}

Result<VideoFrame, Error> Decoder::decodeVideoFrame(Timestamp time) {
    return m_impl->decodeVideo(time, false);
}

Result<VideoFrame, Error> Decoder::decodeNextVideoFrame() {
    return m_impl->decodeVideo(kNoTimestamp, true);
}

Result<AudioFrame, Error> Decoder::decodeAudioFrame(Timestamp time) {
    return m_impl->decodeAudio(time, false);
}

Result<AudioFrame, Error> Decoder::decodeNextAudioFrame() {
    return m_impl->decodeAudio(kNoTimestamp, true);
}

Result<void, Error> Decoder::seek(Timestamp time) {
    return m_impl->seekTo(time);
}

Result<void, Error> Decoder::seekToStart() {
    return seek(0);
}

bool Decoder::isHardwareAccelerated() const {
    return m_impl->activeHWType != HWAccelType::None;
}

HWAccelType Decoder::hwAccelType() const {
    return m_impl->activeHWType;
}

bool Decoder::hasVideo() const {
    return m_impl->videoStreamIdx >= 0;
}

bool Decoder::hasAudio() const {
    return m_impl->audioStreamIdx >= 0;
}

Duration Decoder::duration() const {
    if (!m_impl->formatCtx) return 0;
    if (m_impl->formatCtx->duration == AV_NOPTS_VALUE) return 0;
    return av_rescale_q(m_impl->formatCtx->duration, AV_TIME_BASE_Q, {1, 1000000});
}

Size Decoder::resolution() const {
    if (!m_impl->videoCodecCtx) return {0, 0};
    return {m_impl->videoCodecCtx->width, m_impl->videoCodecCtx->height};
}

Rational Decoder::frameRate() const {
    if (!m_impl->formatCtx || m_impl->videoStreamIdx < 0) return {0, 1};
    return ff::fromAVRational(
        m_impl->formatCtx->streams[m_impl->videoStreamIdx]->avg_frame_rate);
}

int Decoder::sampleRate() const {
    if (!m_impl->audioCodecCtx) return 0;
    return m_impl->audioCodecCtx->sample_rate;
}

int Decoder::audioChannels() const {
    if (!m_impl->audioCodecCtx) return 0;
    return m_impl->audioCodecCtx->ch_layout.nb_channels;
}

Timestamp Decoder::position() const {
    return m_impl->currentVideoPos;
}

DecoderStats Decoder::stats() const {
    return m_impl->stats;
}

const std::filesystem::path& Decoder::path() const {
    return m_impl->filePath;
}

} // namespace phoenix::media
