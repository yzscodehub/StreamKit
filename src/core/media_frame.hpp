/**
 * @file media_frame.hpp
 * @brief VideoFrame, AudioFrame, and Packet definitions
 * 
 * Key design:
 * - Packet: Owns compressed data, moves through demuxer to decoder
 * - VideoFrame: Holds decoded frame with SharedAVFrame payload
 * - AudioFrame: Holds decoded audio with SharedAVFrame payload
 * - All types support EOF sentinel values
 * - Serial number for seek atomicity
 */

#pragma once

#include <vector>
#include <variant>
#include <memory>
#include <functional>
#include <cstdint>

#include "types.hpp"
#include "codec/ffmpeg/shared_avframe.hpp"

namespace phoenix {

// ============================================================================
// Packet - Compressed media data from demuxer
// ============================================================================

/**
 * @brief Compressed packet from demuxer
 * 
 * Owns its data via std::vector (copies from FFmpeg's internal buffers).
 * This decouples our pipeline from FFmpeg's buffer management.
 */
struct Packet {
    /// Compressed data (owned)
    std::vector<uint8_t> data;
    
    /// Presentation timestamp (microseconds)
    Timestamp pts = kNoTimestamp;
    
    /// Decode timestamp (microseconds)
    Timestamp dts = kNoTimestamp;
    
    /// Duration (microseconds)
    Duration duration = 0;
    
    /// Stream index in container
    int streamIndex = -1;
    
    /// Serial number for seek atomicity (frames from old seeks are dropped)
    uint64_t serial = 0;
    
    /// Is this a keyframe?
    bool keyFrame = false;
    
    /// Media type (video/audio)
    MediaType mediaType = MediaType::Unknown;
    
    // ========== EOF Sentinel ==========
    
    /// Create EOF packet
    static Packet eof(uint64_t serial = 0) {
        Packet p;
        p.streamIndex = -1;
        p.serial = serial;
        p.pts = kNoTimestamp;
        return p;
    }
    
    /// Check if this is EOF
    [[nodiscard]] bool isEof() const {
        return data.empty() && streamIndex == -1;
    }
    
    // ========== Utility ==========
    
    /// Get data size
    [[nodiscard]] size_t size() const { return data.size(); }
    
    /// Check if packet has data
    [[nodiscard]] bool hasData() const { return !data.empty(); }
};

// ============================================================================
// Frame Payload Types
// ============================================================================

/// Empty frame (used for EOF or error)
struct EmptyFrame {};

/// Software frame payload (CPU memory)
struct SoftwareFrame {
    SharedAVFrame avFrame;
    
    /// Get plane data pointer
    [[nodiscard]] uint8_t* data(int plane) const {
        return avFrame.get() ? avFrame.get()->data[plane] : nullptr;
    }
    
    /// Get plane linesize
    [[nodiscard]] int linesize(int plane) const {
        return avFrame.get() ? avFrame.get()->linesize[plane] : 0;
    }
};

/// Hardware frame payload (GPU memory)
struct HardwareFrame {
    SharedAVFrame avFrame;
    
    /// Hardware context (keeps context alive while frame exists)
    std::shared_ptr<void> hwContext;
    
    /// Get native handle (ID3D11Texture2D*, etc.)
    [[nodiscard]] void* nativeHandle() const {
        return avFrame.get() ? avFrame.get()->data[3] : nullptr;
    }
};

/// Frame payload variant
using FramePayload = std::variant<EmptyFrame, SoftwareFrame, HardwareFrame>;

// ============================================================================
// VideoFrame
// ============================================================================

/**
 * @brief Decoded video frame
 * 
 * Uses std::variant for payload to support both software and hardware frames.
 * Supports optional recycler callback for object pool integration.
 */
class VideoFrame {
public:
    /// Presentation timestamp (microseconds)
    Timestamp pts = kNoTimestamp;
    
    /// Decode timestamp (microseconds)  
    Timestamp dts = kNoTimestamp;
    
    /// Frame duration (microseconds)
    Duration duration = 0;
    
    /// Frame dimensions
    int width = 0;
    int height = 0;
    
    /// Pixel format
    PixelFormat format = PixelFormat::Unknown;
    
    /// Serial number for seek atomicity
    uint64_t serial = 0;
    
    /// Frame payload (software, hardware, or empty)
    FramePayload payload;
    
    // ========== Constructors ==========
    
    VideoFrame() : payload(EmptyFrame{}) {}
    
    explicit VideoFrame(SoftwareFrame frame) : payload(std::move(frame)) {}
    explicit VideoFrame(HardwareFrame frame) : payload(std::move(frame)) {}
    
    // Move only (due to potential recycler)
    VideoFrame(VideoFrame&&) noexcept = default;
    VideoFrame& operator=(VideoFrame&&) noexcept = default;
    
    // Delete copy
    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;
    
    // ========== Factory Methods ==========
    
    /// Create EOF frame
    static VideoFrame eof(uint64_t serial = 0) {
        VideoFrame f;
        f.serial = serial;
        f.pts = kNoTimestamp;
        return f;
    }
    
    /// Create error frame
    static VideoFrame error(ErrorCode code) {
        VideoFrame f;
        f.pts = static_cast<Timestamp>(code);  // Store error code in pts
        f.width = -1;  // Marker for error
        return f;
    }
    
    /// Create from AVFrame (software)
    static VideoFrame fromAVFrame(const AVFrame* avf, uint64_t serial = 0) {
        VideoFrame f;
        f.pts = avf->pts;  // Will be converted by caller
        f.width = avf->width;
        f.height = avf->height;
        f.format = PixelFormat::Unknown;  // Will be set by caller
        f.serial = serial;
        
        SoftwareFrame sw;
        sw.avFrame = SharedAVFrame::fromRef(avf);
        f.payload = std::move(sw);
        
        return f;
    }
    
    // ========== State Queries ==========
    
    /// Check if this is EOF
    [[nodiscard]] bool isEof() const {
        return std::holds_alternative<EmptyFrame>(payload) && 
               pts == kNoTimestamp && width == 0;
    }
    
    /// Check if this is an error frame
    [[nodiscard]] bool isError() const {
        return width == -1;
    }
    
    /// Check if payload is software frame
    [[nodiscard]] bool isSoftware() const {
        return std::holds_alternative<SoftwareFrame>(payload);
    }
    
    /// Check if payload is hardware frame
    [[nodiscard]] bool isHardware() const {
        return std::holds_alternative<HardwareFrame>(payload);
    }
    
    /// Check if frame has valid data
    [[nodiscard]] bool hasData() const {
        if (auto* sw = std::get_if<SoftwareFrame>(&payload)) {
            return sw->avFrame.hasData();
        }
        if (auto* hw = std::get_if<HardwareFrame>(&payload)) {
            return hw->avFrame.hasData();
        }
        return false;
    }
    
    // ========== Payload Access ==========
    
    /// Get software frame (throws if not software)
    [[nodiscard]] SoftwareFrame& asSoftware() {
        return std::get<SoftwareFrame>(payload);
    }
    
    [[nodiscard]] const SoftwareFrame& asSoftware() const {
        return std::get<SoftwareFrame>(payload);
    }
    
    /// Get hardware frame (throws if not hardware)
    [[nodiscard]] HardwareFrame& asHardware() {
        return std::get<HardwareFrame>(payload);
    }
    
    [[nodiscard]] const HardwareFrame& asHardware() const {
        return std::get<HardwareFrame>(payload);
    }
    
    /// Get underlying AVFrame (works for both software and hardware)
    [[nodiscard]] AVFrame* getAVFrame() {
        if (auto* sw = std::get_if<SoftwareFrame>(&payload)) {
            return sw->avFrame.get();
        }
        if (auto* hw = std::get_if<HardwareFrame>(&payload)) {
            return hw->avFrame.get();
        }
        return nullptr;
    }
    
    [[nodiscard]] const AVFrame* getAVFrame() const {
        if (auto* sw = std::get_if<SoftwareFrame>(&payload)) {
            return sw->avFrame.get();
        }
        if (auto* hw = std::get_if<HardwareFrame>(&payload)) {
            return hw->avFrame.get();
        }
        return nullptr;
    }
};

// ============================================================================
// AudioFrame
// ============================================================================

/**
 * @brief Decoded audio frame
 */
class AudioFrame {
public:
    /// Presentation timestamp (microseconds)
    Timestamp pts = kNoTimestamp;
    
    /// Duration (microseconds)
    Duration duration = 0;
    
    /// Sample rate (Hz)
    int sampleRate = 0;
    
    /// Number of samples
    int nbSamples = 0;
    
    /// Number of channels
    int channels = 0;
    
    /// Sample format
    SampleFormat format = SampleFormat::Unknown;
    
    /// Serial number for seek atomicity
    uint64_t serial = 0;
    
    /// Audio data
    SharedAVFrame avFrame;
    
    // ========== Constructors ==========
    
    AudioFrame() = default;
    
    AudioFrame(AudioFrame&&) noexcept = default;
    AudioFrame& operator=(AudioFrame&&) noexcept = default;
    
    AudioFrame(const AudioFrame&) = delete;
    AudioFrame& operator=(const AudioFrame&) = delete;
    
    // ========== Factory Methods ==========
    
    /// Create EOF frame
    static AudioFrame eof(uint64_t serial = 0) {
        AudioFrame f;
        f.serial = serial;
        f.pts = kNoTimestamp;
        return f;
    }
    
    /// Create from AVFrame
    static AudioFrame fromAVFrame(const AVFrame* avf, uint64_t serial = 0) {
        AudioFrame f;
        f.pts = avf->pts;  // Will be converted by caller
        f.sampleRate = avf->sample_rate;
        f.nbSamples = avf->nb_samples;
        f.channels = avf->ch_layout.nb_channels;
        f.serial = serial;
        f.avFrame = SharedAVFrame::fromRef(avf);
        return f;
    }
    
    // ========== State Queries ==========
    
    /// Check if this is EOF
    [[nodiscard]] bool isEof() const {
        return !avFrame.hasData() && pts == kNoTimestamp && !isError_;
    }
    
    /// Check if this is an error frame
    [[nodiscard]] bool isError() const {
        return isError_;
    }
    
    /// Mark as error frame
    void markError() {
        isError_ = true;
    }
    
    /// Create error frame
    static AudioFrame error(uint64_t serial = 0) {
        AudioFrame f;
        f.serial = serial;
        f.isError_ = true;
        return f;
    }
    
    /// Check if frame has data
    [[nodiscard]] bool hasData() const {
        return avFrame.hasData();
    }
    
    // ========== Data Access ==========
    
    /// Get plane data pointer
    [[nodiscard]] uint8_t* data(int plane = 0) const {
        return avFrame.get() ? avFrame.get()->data[plane] : nullptr;
    }
    
    /// Get plane linesize
    [[nodiscard]] int linesize(int plane = 0) const {
        return avFrame.get() ? avFrame.get()->linesize[plane] : 0;
    }
    
private:
    bool isError_ = false;
};

} // namespace phoenix

