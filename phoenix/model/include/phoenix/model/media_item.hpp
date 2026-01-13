/**
 * @file media_item.hpp
 * @brief Media item representing imported media assets
 * 
 * MediaItem stores metadata about imported media files (video, audio, images).
 * It does not hold the actual media data, only references to the file.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/uuid.hpp>
#include <string>
#include <filesystem>
#include <chrono>

namespace phoenix::model {

/**
 * @brief Type of media asset
 */
enum class MediaItemType {
    Unknown = 0,
    Video,      // Video file (may include audio)
    Audio,      // Audio-only file
    Image,      // Still image
    Sequence,   // Image sequence
};

/**
 * @brief Video stream properties
 */
struct VideoProperties {
    Size resolution{0, 0};
    Rational frameRate{0, 1};
    PixelFormat pixelFormat = PixelFormat::Unknown;
    std::string codec;
    int64_t bitrate = 0;  // bits per second
};

/**
 * @brief Audio stream properties
 */
struct AudioProperties {
    int sampleRate = 0;
    int channels = 0;
    SampleFormat sampleFormat = SampleFormat::Unknown;
    std::string codec;
    int64_t bitrate = 0;  // bits per second
};

/**
 * @brief Imported media item
 * 
 * Represents a media file imported into the project. Multiple clips
 * can reference the same MediaItem.
 */
class MediaItem {
public:
    MediaItem() : m_id(UUID::generate()) {}
    
    explicit MediaItem(const std::filesystem::path& path)
        : m_id(UUID::generate())
        , m_path(path)
        , m_name(path.stem().string())
    {}
    
    // ========== Identification ==========
    
    /// Unique identifier
    [[nodiscard]] const UUID& id() const { return m_id; }
    
    /// Display name (editable by user)
    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    // ========== File Information ==========
    
    /// Absolute path to the media file
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }
    void setPath(const std::filesystem::path& path) { m_path = path; }
    
    /// File size in bytes
    [[nodiscard]] uint64_t fileSize() const { return m_fileSize; }
    void setFileSize(uint64_t size) { m_fileSize = size; }
    
    /// Last modified time
    [[nodiscard]] std::chrono::system_clock::time_point lastModified() const {
        return m_lastModified;
    }
    void setLastModified(std::chrono::system_clock::time_point time) {
        m_lastModified = time;
    }
    
    // ========== Media Properties ==========
    
    /// Media type
    [[nodiscard]] MediaItemType type() const { return m_type; }
    void setType(MediaItemType type) { m_type = type; }
    
    /// Total duration in microseconds
    [[nodiscard]] Duration duration() const { return m_duration; }
    void setDuration(Duration duration) { m_duration = duration; }
    
    /// Check if media has video stream
    [[nodiscard]] bool hasVideo() const { return m_hasVideo; }
    void setHasVideo(bool hasVideo) { m_hasVideo = hasVideo; }
    
    /// Check if media has audio stream
    [[nodiscard]] bool hasAudio() const { return m_hasAudio; }
    void setHasAudio(bool hasAudio) { m_hasAudio = hasAudio; }
    
    /// Video properties (valid if hasVideo)
    [[nodiscard]] const VideoProperties& videoProperties() const { return m_video; }
    VideoProperties& videoProperties() { return m_video; }
    
    /// Audio properties (valid if hasAudio)
    [[nodiscard]] const AudioProperties& audioProperties() const { return m_audio; }
    AudioProperties& audioProperties() { return m_audio; }
    
    // ========== Thumbnail ==========
    
    /// Path to cached thumbnail (may be empty)
    [[nodiscard]] const std::filesystem::path& thumbnailPath() const {
        return m_thumbnailPath;
    }
    void setThumbnailPath(const std::filesystem::path& path) {
        m_thumbnailPath = path;
    }
    
    // ========== Status ==========
    
    /// Check if the file exists and is accessible
    [[nodiscard]] bool isOnline() const {
        return std::filesystem::exists(m_path);
    }
    
    /// Check if media info has been probed
    [[nodiscard]] bool isProbed() const { return m_probed; }
    void setProbed(bool probed) { m_probed = probed; }
    
    // ========== Convenience ==========
    
    /// Get frame count (for video)
    [[nodiscard]] int64_t frameCount() const {
        if (!m_hasVideo || m_video.frameRate.den == 0) return 0;
        double fps = m_video.frameRate.toDouble();
        double seconds = static_cast<double>(m_duration) / kTimeBaseUs;
        return static_cast<int64_t>(seconds * fps);
    }
    
    /// Get duration as string (HH:MM:SS.mmm)
    [[nodiscard]] std::string durationString() const {
        int64_t totalMs = m_duration / 1000;
        int hours = static_cast<int>(totalMs / 3600000);
        int minutes = static_cast<int>((totalMs % 3600000) / 60000);
        int seconds = static_cast<int>((totalMs % 60000) / 1000);
        int ms = static_cast<int>(totalMs % 1000);
        
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                      hours, minutes, seconds, ms);
        return buf;
    }
    
private:
    // Identification
    UUID m_id;
    std::string m_name;
    
    // File info
    std::filesystem::path m_path;
    uint64_t m_fileSize = 0;
    std::chrono::system_clock::time_point m_lastModified;
    
    // Media properties
    MediaItemType m_type = MediaItemType::Unknown;
    Duration m_duration = 0;
    bool m_hasVideo = false;
    bool m_hasAudio = false;
    VideoProperties m_video;
    AudioProperties m_audio;
    
    // Cached data
    std::filesystem::path m_thumbnailPath;
    bool m_probed = false;
};

} // namespace phoenix::model
