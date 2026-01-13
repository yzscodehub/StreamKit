/**
 * @file decoder.hpp
 * @brief Video/Audio decoder interface (PIMPL)
 * 
 * Provides a high-level decoder interface that hides FFmpeg
 * implementation details. Supports both software and hardware
 * accelerated decoding.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/codec_types.hpp>
#include <phoenix/media/frame.hpp>
#include <memory>
#include <string>
#include <filesystem>

namespace phoenix::media {

/**
 * @brief Decoder configuration
 */
struct DecoderConfig {
    std::filesystem::path path;                    // Media file path
    CodecPreference codecPreference = CodecPreference::Auto;
    HWAccelType forceHWType = HWAccelType::None;   // Only if ForceHW
    DecodeMode mode = DecodeMode::RandomAccess;    // For editing
    int threadCount = 0;                           // 0 = auto
};

/**
 * @brief Decoder statistics
 */
struct DecoderStats {
    uint64_t framesDecoded = 0;
    uint64_t seekCount = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    double avgDecodeTimeMs = 0.0;
};

/**
 * @brief Media decoder (PIMPL)
 * 
 * Decodes video and audio frames from media files. The implementation
 * is hidden to isolate FFmpeg dependencies.
 * 
 * Usage:
 * @code
 *   Decoder decoder;
 *   decoder.open("video.mp4");
 *   
 *   // Seek and decode
 *   auto frame = decoder.decodeVideoFrame(1000000);  // 1 second
 *   if (frame) {
 *       // Use frame
 *   }
 * @endcode
 */
class Decoder {
public:
    Decoder();
    ~Decoder();
    
    // Move only (not copyable)
    Decoder(Decoder&& other) noexcept;
    Decoder& operator=(Decoder&& other) noexcept;
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    
    // ========== Open/Close ==========
    
    /**
     * @brief Open media file with configuration
     * 
     * @param config Decoder configuration
     * @return Success or error
     */
    Result<void, Error> open(const DecoderConfig& config);
    
    /**
     * @brief Open media file with default configuration
     * 
     * @param path Path to media file
     * @return Success or error
     */
    Result<void, Error> open(const std::filesystem::path& path);
    
    /**
     * @brief Close the decoder and release resources
     */
    void close();
    
    /**
     * @brief Check if decoder is open
     */
    [[nodiscard]] bool isOpen() const;
    
    // ========== Video Decoding ==========
    
    /**
     * @brief Decode video frame at specific time
     * 
     * Seeks to the nearest keyframe and decodes to the target time.
     * For random access, this is the recommended method.
     * 
     * @param time Target time in microseconds
     * @return Decoded frame or error
     */
    Result<VideoFrame, Error> decodeVideoFrame(Timestamp time);
    
    /**
     * @brief Decode next sequential video frame
     * 
     * For streaming playback. Returns the next frame after the
     * previously decoded frame.
     * 
     * @return Decoded frame or error
     */
    Result<VideoFrame, Error> decodeNextVideoFrame();
    
    // ========== Audio Decoding ==========
    
    /**
     * @brief Decode audio at specific time
     * 
     * @param time Target time in microseconds
     * @return Decoded audio frame or error
     */
    Result<AudioFrame, Error> decodeAudioFrame(Timestamp time);
    
    /**
     * @brief Decode next sequential audio frame
     * 
     * @return Decoded audio frame or error
     */
    Result<AudioFrame, Error> decodeNextAudioFrame();
    
    // ========== Seeking ==========
    
    /**
     * @brief Seek to specific time
     * 
     * @param time Target time in microseconds
     * @return Success or error
     */
    Result<void, Error> seek(Timestamp time);
    
    /**
     * @brief Seek to start of file
     */
    Result<void, Error> seekToStart();
    
    // ========== Properties ==========
    
    /// Check if hardware acceleration is active
    [[nodiscard]] bool isHardwareAccelerated() const;
    
    /// Get current hardware acceleration type
    [[nodiscard]] HWAccelType hwAccelType() const;
    
    /// Check if file has video stream
    [[nodiscard]] bool hasVideo() const;
    
    /// Check if file has audio stream
    [[nodiscard]] bool hasAudio() const;
    
    /// Get video duration
    [[nodiscard]] Duration duration() const;
    
    /// Get video resolution
    [[nodiscard]] Size resolution() const;
    
    /// Get video frame rate
    [[nodiscard]] Rational frameRate() const;
    
    /// Get audio sample rate
    [[nodiscard]] int sampleRate() const;
    
    /// Get audio channel count
    [[nodiscard]] int audioChannels() const;
    
    /// Get current position
    [[nodiscard]] Timestamp position() const;
    
    /// Get statistics
    [[nodiscard]] DecoderStats stats() const;
    
    /// Get file path
    [[nodiscard]] const std::filesystem::path& path() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace phoenix::media
