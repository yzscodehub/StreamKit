/**
 * @file frame.hpp
 * @brief Video and audio frame types
 * 
 * Provides VideoFrame and AudioFrame classes that abstract
 * away FFmpeg details from the public API. Frames support
 * both software (CPU) and hardware (GPU) data.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/codec_types.hpp>
#include <memory>
#include <cstdint>

namespace phoenix::media {

/**
 * @brief Video frame (supports CPU and GPU data)
 * 
 * VideoFrame wraps decoded video data. It can hold either:
 * - Software frame: Data in CPU memory (accessible via data())
 * - Hardware frame: Data in GPU memory (must transfer before access)
 * 
 * The frame uses shared ownership (copy shares the same data).
 */
class VideoFrame {
public:
    VideoFrame();
    ~VideoFrame();
    
    // Copy and move
    VideoFrame(const VideoFrame& other);
    VideoFrame& operator=(const VideoFrame& other);
    VideoFrame(VideoFrame&& other) noexcept;
    VideoFrame& operator=(VideoFrame&& other) noexcept;
    
    // ========== Properties ==========
    
    /// Frame width in pixels
    [[nodiscard]] int width() const;
    
    /// Frame height in pixels
    [[nodiscard]] int height() const;
    
    /// Pixel format
    [[nodiscard]] PixelFormat format() const;
    
    /// Presentation timestamp (microseconds)
    [[nodiscard]] Timestamp pts() const;
    
    /// Duration (microseconds)
    [[nodiscard]] Duration duration() const;
    
    /// Frame number (if known)
    [[nodiscard]] int64_t frameNumber() const;
    
    // ========== Hardware Frame Info ==========
    
    /// Check if this is a hardware frame
    [[nodiscard]] bool isHardwareFrame() const;
    
    /// Get hardware acceleration type
    [[nodiscard]] HWAccelType hwAccelType() const;
    
    /**
     * @brief Transfer hardware frame to CPU memory
     * 
     * If the frame is already in CPU memory, returns a copy.
     * 
     * @return Software frame or error
     */
    [[nodiscard]] Result<VideoFrame, Error> transferToCPU() const;
    
    // ========== Data Access (CPU frames only) ==========
    
    /// Get plane count
    [[nodiscard]] int planeCount() const;
    
    /// Get data pointer for plane (nullptr if hardware frame)
    [[nodiscard]] const uint8_t* data(int plane) const;
    
    /// Get mutable data pointer (nullptr if hardware frame)
    [[nodiscard]] uint8_t* data(int plane);
    
    /// Get line size (stride) for plane
    [[nodiscard]] int linesize(int plane) const;
    
    // ========== Validity ==========
    
    /// Check if frame contains valid data
    [[nodiscard]] bool isValid() const;
    
    /// Explicit bool conversion
    explicit operator bool() const { return isValid(); }
    
    // ========== Factory ==========
    
    /**
     * @brief Create an empty frame with specified dimensions
     * 
     * @param width Frame width
     * @param height Frame height
     * @param format Pixel format
     * @return Allocated frame or error
     */
    static Result<VideoFrame, Error> create(
        int width, int height, PixelFormat format);
    
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
    
    // Allow Decoder to construct frames
    friend class Decoder;
    friend class DecoderImpl;
};

/**
 * @brief Audio frame
 * 
 * Contains decoded audio samples in a specific format.
 */
class AudioFrame {
public:
    AudioFrame();
    ~AudioFrame();
    
    // Copy and move
    AudioFrame(const AudioFrame& other);
    AudioFrame& operator=(const AudioFrame& other);
    AudioFrame(AudioFrame&& other) noexcept;
    AudioFrame& operator=(AudioFrame&& other) noexcept;
    
    // ========== Properties ==========
    
    /// Sample rate (Hz)
    [[nodiscard]] int sampleRate() const;
    
    /// Number of channels
    [[nodiscard]] int channels() const;
    
    /// Number of samples per channel
    [[nodiscard]] int sampleCount() const;
    
    /// Sample format
    [[nodiscard]] SampleFormat format() const;
    
    /// Presentation timestamp (microseconds)
    [[nodiscard]] Timestamp pts() const;
    
    /// Duration (microseconds)
    [[nodiscard]] Duration duration() const;
    
    // ========== Data Access ==========
    
    /// Check if format is planar
    [[nodiscard]] bool isPlanar() const;
    
    /// Get data pointer (for interleaved) or plane pointer (for planar)
    [[nodiscard]] const uint8_t* data(int plane = 0) const;
    
    /// Get mutable data pointer
    [[nodiscard]] uint8_t* data(int plane = 0);
    
    /// Get line size (buffer size for non-planar, per-plane for planar)
    [[nodiscard]] int linesize(int plane = 0) const;
    
    /// Get total data size in bytes
    [[nodiscard]] size_t dataSize() const;
    
    // ========== Validity ==========
    
    [[nodiscard]] bool isValid() const;
    explicit operator bool() const { return isValid(); }
    
    // ========== Factory ==========
    
    /**
     * @brief Create an empty audio frame
     */
    static Result<AudioFrame, Error> create(
        int sampleCount, int channels, int sampleRate, SampleFormat format);
    
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
    
    friend class Decoder;
    friend class DecoderImpl;
};

} // namespace phoenix::media
