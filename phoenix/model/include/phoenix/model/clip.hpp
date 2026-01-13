/**
 * @file clip.hpp
 * @brief Clip representing a segment of media on the timeline
 * 
 * A Clip is a reference to a portion of a MediaItem, placed at a
 * specific position on a Track.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/uuid.hpp>
#include <string>
#include <vector>

namespace phoenix::model {

// Forward declaration
struct EffectInstance;

/**
 * @brief Clip type
 */
enum class ClipType {
    Video,      // Video clip (may include linked audio)
    Audio,      // Audio-only clip
    Title,      // Text/graphics generator
    Adjustment, // Adjustment layer
};

/**
 * @brief A clip on the timeline
 * 
 * Represents a segment of source media placed at a specific
 * position on the timeline. Clips reference MediaItems but
 * can have different in/out points and effects.
 * 
 * Timeline coordinates:
 * - timelineIn: Start position on timeline (inclusive)
 * - timelineOut: End position on timeline (exclusive)
 * 
 * Source coordinates:
 * - sourceIn: Start position in source media
 * - sourceOut: End position in source media
 */
class Clip {
public:
    Clip() : m_id(UUID::generate()) {}
    
    explicit Clip(const UUID& mediaItemId)
        : m_id(UUID::generate())
        , m_mediaItemId(mediaItemId)
    {}
    
    Clip(const UUID& clipId, const UUID& mediaItemId)
        : m_id(clipId)
        , m_mediaItemId(mediaItemId)
    {}
    
    // ========== Identification ==========
    
    /// Unique clip ID
    [[nodiscard]] const UUID& id() const { return m_id; }
    
    /// Reference to source MediaItem
    [[nodiscard]] const UUID& mediaItemId() const { return m_mediaItemId; }
    void setMediaItemId(const UUID& id) { m_mediaItemId = id; }
    
    /// Display name (optional, uses media name if empty)
    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    /// Clip type
    [[nodiscard]] ClipType type() const { return m_type; }
    void setType(ClipType type) { m_type = type; }
    
    // ========== Timeline Position ==========
    
    /// Start position on timeline (microseconds)
    [[nodiscard]] Timestamp timelineIn() const { return m_timelineIn; }
    void setTimelineIn(Timestamp time) { m_timelineIn = time; }
    
    /// End position on timeline (microseconds)
    [[nodiscard]] Timestamp timelineOut() const { return m_timelineOut; }
    void setTimelineOut(Timestamp time) { m_timelineOut = time; }
    
    /// Duration on timeline
    [[nodiscard]] Duration duration() const {
        return m_timelineOut - m_timelineIn;
    }
    
    /// Set duration (adjusts timelineOut)
    void setDuration(Duration dur) {
        m_timelineOut = m_timelineIn + dur;
    }
    
    // ========== Source Position ==========
    
    /// Start position in source media (microseconds)
    [[nodiscard]] Timestamp sourceIn() const { return m_sourceIn; }
    void setSourceIn(Timestamp time) { m_sourceIn = time; }
    
    /// End position in source media (microseconds)
    [[nodiscard]] Timestamp sourceOut() const { return m_sourceOut; }
    void setSourceOut(Timestamp time) { m_sourceOut = time; }
    
    /// Source duration (before speed adjustment)
    [[nodiscard]] Duration sourceDuration() const {
        return m_sourceOut - m_sourceIn;
    }
    
    // ========== Speed & Time Mapping ==========
    
    /// Playback speed (1.0 = normal, 2.0 = 2x, 0.5 = half)
    [[nodiscard]] float speed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = speed; }
    
    /// Reverse playback
    [[nodiscard]] bool reversed() const { return m_reversed; }
    void setReversed(bool reversed) { m_reversed = reversed; }
    
    /**
     * @brief Map timeline time to source time
     * 
     * @param timelineTime Absolute timeline position
     * @return Corresponding source media position
     */
    [[nodiscard]] Timestamp mapToSource(Timestamp timelineTime) const {
        // Clamp to clip range
        if (timelineTime < m_timelineIn) timelineTime = m_timelineIn;
        if (timelineTime > m_timelineOut) timelineTime = m_timelineOut;
        
        // Calculate offset from clip start
        Duration offset = timelineTime - m_timelineIn;
        
        // Apply speed
        Duration sourceOffset = static_cast<Duration>(offset * m_speed);
        
        // Apply reverse
        if (m_reversed) {
            return m_sourceOut - sourceOffset;
        } else {
            return m_sourceIn + sourceOffset;
        }
    }
    
    /**
     * @brief Check if timeline time falls within this clip
     */
    [[nodiscard]] bool containsTime(Timestamp time) const {
        return time >= m_timelineIn && time < m_timelineOut;
    }
    
    // ========== Visual Properties ==========
    
    /// Opacity (0.0 - 1.0)
    [[nodiscard]] float opacity() const { return m_opacity; }
    void setOpacity(float opacity) { m_opacity = opacity; }
    
    /// Volume (0.0 - 1.0, can exceed 1.0 for boost)
    [[nodiscard]] float volume() const { return m_volume; }
    void setVolume(float volume) { m_volume = volume; }
    
    /// Muted state
    [[nodiscard]] bool muted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }
    
    /// Disabled (excluded from playback)
    [[nodiscard]] bool disabled() const { return m_disabled; }
    void setDisabled(bool disabled) { m_disabled = disabled; }
    
    // ========== Track Association ==========
    
    /// Track index (set by Track when clip is added)
    [[nodiscard]] int trackIndex() const { return m_trackIndex; }
    void setTrackIndex(int index) { m_trackIndex = index; }
    
    // ========== Selection State ==========
    
    /// Selection state (UI only, not serialized)
    [[nodiscard]] bool selected() const { return m_selected; }
    void setSelected(bool selected) { m_selected = selected; }
    
    // ========== Comparison ==========
    
    bool operator==(const Clip& other) const {
        return m_id == other.m_id;
    }
    
    bool operator!=(const Clip& other) const {
        return !(*this == other);
    }
    
    /// Compare by timeline position (for sorting)
    bool operator<(const Clip& other) const {
        return m_timelineIn < other.m_timelineIn;
    }
    
private:
    // Identification
    UUID m_id;
    UUID m_mediaItemId;
    std::string m_name;
    ClipType m_type = ClipType::Video;
    
    // Timeline position
    Timestamp m_timelineIn = 0;
    Timestamp m_timelineOut = 0;
    
    // Source position
    Timestamp m_sourceIn = 0;
    Timestamp m_sourceOut = 0;
    
    // Time mapping
    float m_speed = 1.0f;
    bool m_reversed = false;
    
    // Visual/audio properties
    float m_opacity = 1.0f;
    float m_volume = 1.0f;
    bool m_muted = false;
    bool m_disabled = false;
    
    // Track association
    int m_trackIndex = -1;
    
    // UI state (transient)
    bool m_selected = false;
};

} // namespace phoenix::model
