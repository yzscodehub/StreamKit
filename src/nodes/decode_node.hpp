/**
 * @file decode_node.hpp
 * @brief FFmpegDecodeNode - Decodes video/audio packets to frames
 * 
 * CRITICAL REQUIREMENTS:
 * 1. process() returns void, may emit 0..N frames
 * 2. EOF handling: avcodec_send_packet(NULL) then loop receive until AVERROR_EOF
 * 3. Iteration limit to prevent infinite loops
 * 4. flushCodec() calls avcodec_flush_buffers() for seek
 * 5. EAGAIN is normal, not an error
 * 6. Error recovery: skip bad packets, fail after N consecutive errors
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
 * @brief FFmpeg video decoder node
 * 
 * Decodes video packets to VideoFrames using FFmpeg.
 * Supports software decoding (hardware decode in Phase 4).
 */
class FFmpegDecodeNode : public ProcessorNode<Packet, VideoFrame> {
public:
    explicit FFmpegDecodeNode(std::string name = "Decoder",
                              size_t inputCapacity = kDefaultVideoQueueCapacity)
        : ProcessorNode<Packet, VideoFrame>(std::move(name), inputCapacity)
    {}
    
    ~FFmpegDecodeNode() override {
        stop();
    }
    
    // ========== Initialization ==========
    
    /**
     * @brief Initialize decoder for a stream
     * @param stream FFmpeg stream to decode
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
        
        spdlog::info("[{}] Initialized decoder: {} {}x{}",
            name_,
            avcodec_get_name(codecCtx_->codec_id),
            codecCtx_->width, codecCtx_->height);
        
        return Ok();
    }
    
    /**
     * @brief Get codec context (for hardware config, etc.)
     */
    AVCodecContext* codecContext() { return codecCtx_.get(); }
    
    // ========== Lifecycle ==========
    
    void start() override {
        ProcessorNode::start();
        consecutiveErrors_ = 0;
        spdlog::debug("[{}] Started", name_);
    }
    
    void stop() override {
        ProcessorNode::stop();
        spdlog::debug("[{}] Stopped, decoded {} frames, dropped {} packets",
            name_, framesDecoded_.load(), packetsDropped_.load());
    }
    
    /**
     * @brief Flush decoder state
     * 
     * CRITICAL: Must call avcodec_flush_buffers() for seek!
     */
    void flush() override {
        ProcessorNode::flush();
        
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
     * May emit 0..N frames. Handles EOF by draining decoder.
     */
    void process(Packet pkt) override {
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
        // Note: We use a local AVPacket on stack and manually set data pointer
        // This is safe because FFmpeg won't free our data (we don't set buf)
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
                // Decoder full, receive frames first
                receiveFrames(pkt.serial);
                
                // Retry send
                ret = avcodec_send_packet(codecCtx_.get(), &avpkt);
                if (ret < 0 && ret != AVERROR(EAGAIN)) {
                    handleDecodeError(ret);
                    return;
                }
            } else if (ret == AVERROR_INVALIDDATA) {
                // Skip corrupted packet
                handleDecodeError(ret);
                return;
            } else {
                handleDecodeError(ret);
                return;
            }
        }
        
        // No need to free - we used stack-based AVPacket with external data
        
        // Success - reset error counter
        consecutiveErrors_ = 0;
        
        // Receive all available frames
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
                // Need more input - NORMAL, not error
                return;
            }
            
            if (ret == AVERROR_EOF) {
                // Decoder fully drained
                return;
            }
            
            if (ret < 0) {
                spdlog::warn("[{}] receive_frame error: {}", name_, ffmpegErrorString(ret));
                return;
            }
            
            // Emit decoded frame
            emitFrame(frame, serial);
            frame.unref();  // Reuse frame for next iteration
        }
        
        if (iterations >= kMaxDecodeLoopIterations) {
            spdlog::warn("[{}] Hit decode loop iteration limit", name_);
        }
    }
    
    /**
     * @brief Drain decoder on EOF
     * 
     * CRITICAL: Send NULL packet to enter drain mode, then receive all frames.
     */
    void drainDecoder(uint64_t serial) {
        spdlog::debug("[{}] Draining decoder...", name_);
        
        // Enter drain mode
        int ret = avcodec_send_packet(codecCtx_.get(), nullptr);
        if (ret < 0 && ret != AVERROR_EOF) {
            spdlog::warn("[{}] Error entering drain mode: {}", name_, ffmpegErrorString(ret));
        }
        
        // Receive all remaining frames
        SharedAVFrame frame;
        int iterations = 0;
        int drainedFrames = 0;
        
        while (iterations++ < kMaxDecodeLoopIterations) {
            ret = avcodec_receive_frame(codecCtx_.get(), frame.get());
            
            if (ret == AVERROR_EOF) {
                // Decoder fully drained
                break;
            }
            
            if (ret < 0) {
                break;
            }
            
            emitFrame(frame, serial);
            frame.unref();
            drainedFrames++;
        }
        
        spdlog::debug("[{}] Drained {} frames", name_, drainedFrames);
        
        // NOW emit EOF downstream
        emit(VideoFrame::eof(serial));
    }
    
    /**
     * @brief Emit a decoded frame
     */
    void emitFrame(const SharedAVFrame& avFrame, uint64_t serial) {
        VideoFrame frame;
        
        // Metadata
        frame.pts = ptsToMicroseconds(avFrame->pts, timeBase_);
        frame.dts = ptsToMicroseconds(avFrame->pkt_dts, timeBase_);
        frame.duration = ptsToMicroseconds(avFrame->duration, timeBase_);
        frame.width = avFrame->width;
        frame.height = avFrame->height;
        frame.format = toPixelFormat(static_cast<AVPixelFormat>(avFrame->format));
        frame.serial = serial;
        
        // Payload (software frame)
        SoftwareFrame sw;
        sw.avFrame = SharedAVFrame::fromRef(avFrame.get());
        frame.payload = std::move(sw);
        
        // Emit
        auto result = emit(std::move(frame));
        if (!result.ok()) {
            spdlog::warn("[{}] Failed to emit frame: {}", name_, result.error().what());
        }
        
        framesDecoded_.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Handle decode errors
     */
    void handleDecodeError(int ret) {
        consecutiveErrors_++;
        packetsDropped_.fetch_add(1, std::memory_order_relaxed);
        
        if (consecutiveErrors_ >= kMaxConsecutiveDecoderErrors) {
            spdlog::error("[{}] {} consecutive decode errors, emitting error frame",
                name_, consecutiveErrors_);
            emit(VideoFrame::error(ErrorCode::DecoderError));
            consecutiveErrors_ = 0;  // Reset after reporting
        } else {
            spdlog::warn("[{}] Decode error ({}), consecutive: {}",
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

