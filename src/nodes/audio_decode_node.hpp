/**
 * @file audio_decode_node.hpp
 * @brief AudioDecodeNode - Decodes audio packets to AudioFrames
 * 
 * Similar to FFmpegDecodeNode but specialized for audio.
 * Outputs AudioFrame instead of VideoFrame.
 */

#pragma once

#include <atomic>
#include <cstring>

#include "graph/node.hpp"
#include "core/types.hpp"
#include "core/media_frame.hpp"
#include "codec/ffmpeg/ff_utils.hpp"
#include "codec/ffmpeg/shared_avframe.hpp"

#include <spdlog/spdlog.h>

namespace phoenix {

/**
 * @brief FFmpeg audio decoder node
 * 
 * Decodes audio packets to AudioFrames using FFmpeg.
 */
class AudioDecodeNode : public NodeBase {
public:
    explicit AudioDecodeNode(std::string name = "AudioDecoder",
                             size_t inputCapacity = kDefaultAudioQueueCapacity)
        : NodeBase(std::move(name))
        , input(inputCapacity)
    {}
    
    ~AudioDecodeNode() override {
        stop();
    }
    
    /// Input pin for packets
    InputPin<Packet> input;
    
    /// Output pin for audio frames
    OutputPin<AudioFrame> output;
    
    // ========== Initialization ==========
    
    /**
     * @brief Initialize decoder for an audio stream
     * @param stream FFmpeg audio stream
     * @return Ok on success, error otherwise
     */
    Result<void> init(AVStream* stream) {
        if (!stream) {
            return Err(ErrorCode::InvalidArgument, "Null stream");
        }
        
        stream_ = stream;
        timeBase_ = stream->time_base;
        
        // Create decoder context
        auto result = createDecoderContext(stream);
        if (!result.ok()) {
            return Err(result.error());
        }
        codecCtx_ = std::move(result.value());
        
        spdlog::info("[{}] Initialized audio decoder: {} {} Hz, {} channels",
            name_,
            avcodec_get_name(codecCtx_->codec_id),
            codecCtx_->sample_rate,
            codecCtx_->ch_layout.nb_channels);
        
        return Ok();
    }
    
    /**
     * @brief Get codec context
     */
    AVCodecContext* codecContext() { return codecCtx_.get(); }
    
    // ========== Lifecycle ==========
    
    void start() override {
        if (running_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        
        consecutiveErrors_ = 0;
        input.reset();
        
        spdlog::debug("[{}] Started", name_);
    }
    
    void stop() override {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        
        input.stop();
        
        spdlog::debug("[{}] Stopped, decoded {} frames, dropped {} packets",
            name_, framesDecoded_.load(), packetsDropped_.load());
    }
    
    /**
     * @brief Flush decoder state
     */
    void flush() override {
        input.flush();
        
        if (codecCtx_) {
            avcodec_flush_buffers(codecCtx_.get());
            spdlog::debug("[{}] Flushed codec buffers", name_);
        }
        
        consecutiveErrors_ = 0;
    }
    
    // ========== Processing ==========
    
    /**
     * @brief Process one packet
     * 
     * May emit 0..N frames.
     */
    void process(Packet pkt) {
        if (!codecCtx_) {
            spdlog::error("[{}] Not initialized", name_);
            return;
        }
        
        // Handle EOF
        if (pkt.isEof()) {
            drainDecoder(pkt.serial);
            return;
        }
        
        // Create AVPacket from our Packet
        AVPacket avpkt;
        memset(&avpkt, 0, sizeof(avpkt));
        avpkt.data = pkt.data.data();
        avpkt.size = static_cast<int>(pkt.data.size());
        avpkt.pts = microsecondsToPts(pkt.pts, timeBase_);
        avpkt.dts = microsecondsToPts(pkt.dts, timeBase_);
        avpkt.duration = microsecondsToPts(pkt.duration, timeBase_);
        
        // Send packet to decoder
        int ret = avcodec_send_packet(codecCtx_.get(), &avpkt);
        
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                receiveFrames(pkt.serial);
                ret = avcodec_send_packet(codecCtx_.get(), &avpkt);
                if (ret < 0 && ret != AVERROR(EAGAIN)) {
                    handleDecodeError(ret);
                    return;
                }
            } else if (ret == AVERROR_INVALIDDATA) {
                handleDecodeError(ret);
                return;
            } else {
                handleDecodeError(ret);
                return;
            }
        }
        
        consecutiveErrors_ = 0;
        receiveFrames(pkt.serial);
    }
    
    // ========== Statistics ==========
    
    [[nodiscard]] uint64_t framesDecoded() const {
        return framesDecoded_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] uint64_t packetsDropped() const {
        return packetsDropped_.load(std::memory_order_relaxed);
    }
    
private:
    /**
     * @brief Receive all available frames from decoder
     */
    void receiveFrames(uint64_t serial) {
        SharedAVFrame frame;
        int iterations = 0;
        
        while (iterations++ < kMaxDecodeLoopIterations) {
            int ret = avcodec_receive_frame(codecCtx_.get(), frame.get());
            
            if (ret == AVERROR(EAGAIN)) {
                return;
            }
            
            if (ret == AVERROR_EOF) {
                return;
            }
            
            if (ret < 0) {
                spdlog::warn("[{}] receive_frame error: {}", name_, ffmpegErrorString(ret));
                return;
            }
            
            emitFrame(frame, serial);
            frame.unref();
        }
        
        if (iterations >= kMaxDecodeLoopIterations) {
            spdlog::warn("[{}] Hit decode loop iteration limit", name_);
        }
    }
    
    /**
     * @brief Drain decoder on EOF
     */
    void drainDecoder(uint64_t serial) {
        spdlog::debug("[{}] Draining audio decoder...", name_);
        
        int ret = avcodec_send_packet(codecCtx_.get(), nullptr);
        if (ret < 0 && ret != AVERROR_EOF) {
            spdlog::warn("[{}] Error entering drain mode: {}", name_, ffmpegErrorString(ret));
        }
        
        SharedAVFrame frame;
        int iterations = 0;
        int drainedFrames = 0;
        
        while (iterations++ < kMaxDecodeLoopIterations) {
            ret = avcodec_receive_frame(codecCtx_.get(), frame.get());
            
            if (ret == AVERROR_EOF) {
                break;
            }
            
            if (ret < 0) {
                break;
            }
            
            emitFrame(frame, serial);
            frame.unref();
            drainedFrames++;
        }
        
        spdlog::debug("[{}] Drained {} audio frames", name_, drainedFrames);
        
        // Emit EOF downstream
        emit(AudioFrame::eof(serial));
    }
    
    /**
     * @brief Emit a decoded audio frame
     */
    void emitFrame(const SharedAVFrame& avFrame, uint64_t serial) {
        AudioFrame frame;
        
        // Metadata
        Timestamp pts = ptsToMicroseconds(avFrame->pts, timeBase_);
        frame.pts = pts;
        frame.duration = ptsToMicroseconds(avFrame->duration, timeBase_);
        frame.sampleRate = avFrame->sample_rate;
        frame.nbSamples = avFrame->nb_samples;
        frame.channels = avFrame->ch_layout.nb_channels;
        frame.format = toSampleFormat(static_cast<AVSampleFormat>(avFrame->format));
        frame.serial = serial;
        
        // Copy the frame using av_frame_ref
        frame.avFrame = SharedAVFrame::fromRef(avFrame.get());
        
        uint64_t count = framesDecoded_.fetch_add(1, std::memory_order_relaxed);
        
        // Log first frame (before move)
        if (count == 0) {
            spdlog::info("[{}] First audio frame decoded: pts={}, samples={}, rate={}, channels={}, format={}",
                name_, pts, avFrame->nb_samples, avFrame->sample_rate,
                avFrame->ch_layout.nb_channels, av_get_sample_fmt_name(static_cast<AVSampleFormat>(avFrame->format)));
        }
        
        // Emit (moves frame)
        emit(std::move(frame));
    }
    
    /**
     * @brief Emit audio frame to output
     */
    void emit(AudioFrame frame) {
        if (output.isConnected()) {
            auto result = output.emit(std::move(frame));
            if (!result.ok() && running_.load(std::memory_order_acquire)) {
                spdlog::warn("[{}] Failed to emit frame", name_);
            }
        }
    }
    
    /**
     * @brief Handle decode errors
     */
    void handleDecodeError(int ret) {
        consecutiveErrors_++;
        packetsDropped_.fetch_add(1, std::memory_order_relaxed);
        
        if (consecutiveErrors_ >= kMaxConsecutiveDecoderErrors) {
            spdlog::error("[{}] {} consecutive decode errors",
                name_, consecutiveErrors_);
            consecutiveErrors_ = 0;
        } else {
            spdlog::warn("[{}] Audio decode error ({}), consecutive: {}",
                name_, ffmpegErrorString(ret), consecutiveErrors_);
        }
    }
    
    // Codec state
    CodecContextPtr codecCtx_;
    AVStream* stream_ = nullptr;
    AVRational timeBase_{1, 1000000};
    
    // Error tracking
    int consecutiveErrors_ = 0;
    
    // Statistics
    std::atomic<uint64_t> framesDecoded_{0};
    std::atomic<uint64_t> packetsDropped_{0};
};

} // namespace phoenix

