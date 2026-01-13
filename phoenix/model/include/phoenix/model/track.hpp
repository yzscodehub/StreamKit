/**
 * @file track.hpp
 * @brief Track containing a sequence of clips
 * 
 * Tracks are horizontal lanes in the timeline that contain clips.
 * Video tracks stack from bottom to top, audio tracks are mixed.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/uuid.hpp>
#include <phoenix/core/signals.hpp>
#include <phoenix/model/clip.hpp>
#include <vector>
#include <memory>
#include <algorithm>
#include <optional>

namespace phoenix::model {

/**
 * @brief Track type
 */
enum class TrackType {
    Video,  // Video track (clips stack visually)
    Audio,  // Audio track (clips are mixed)
};

/**
 * @brief A track containing clips
 * 
 * Clips on a track cannot overlap. The track maintains
 * clips sorted by timeline position.
 */
class Track {
public:
    using ClipPtr = std::shared_ptr<Clip>;
    
    Track() : m_id(UUID::generate()) {}
    
    explicit Track(TrackType type)
        : m_id(UUID::generate())
        , m_type(type)
    {}
    
    // ========== Identification ==========
    
    [[nodiscard]] const UUID& id() const { return m_id; }
    
    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    [[nodiscard]] TrackType type() const { return m_type; }
    void setType(TrackType type) { m_type = type; }
    
    [[nodiscard]] int index() const { return m_index; }
    void setIndex(int index) { m_index = index; }
    
    // ========== Track State ==========
    
    /// Muted (audio not played)
    [[nodiscard]] bool muted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }
    
    /// Locked (cannot be edited)
    [[nodiscard]] bool locked() const { return m_locked; }
    void setLocked(bool locked) { m_locked = locked; }
    
    /// Hidden (not visible in preview)
    [[nodiscard]] bool hidden() const { return m_hidden; }
    void setHidden(bool hidden) { m_hidden = hidden; }
    
    /// Solo (only this track is active)
    [[nodiscard]] bool solo() const { return m_solo; }
    void setSolo(bool solo) { m_solo = solo; }
    
    // ========== Clip Management ==========
    
    /**
     * @brief Add a clip to the track
     * 
     * @param clip Clip to add
     * @return true if added, false if overlaps with existing clip
     */
    bool addClip(ClipPtr clip) {
        if (!clip) return false;
        
        // Check for overlaps
        if (hasOverlap(clip->timelineIn(), clip->timelineOut(), nullptr)) {
            return false;
        }
        
        clip->setTrackIndex(m_index);
        m_clips.push_back(clip);
        sortClips();
        
        clipAdded.fire(clip);
        return true;
    }
    
    /**
     * @brief Remove a clip by ID
     * 
     * @param clipId UUID of clip to remove
     * @return The removed clip, or nullptr if not found
     */
    ClipPtr removeClip(const UUID& clipId) {
        auto it = std::find_if(m_clips.begin(), m_clips.end(),
            [&clipId](const ClipPtr& c) { return c->id() == clipId; });
        
        if (it == m_clips.end()) {
            return nullptr;
        }
        
        ClipPtr clip = *it;
        m_clips.erase(it);
        clip->setTrackIndex(-1);
        
        clipRemoved.fire(clipId);
        return clip;
    }
    
    /**
     * @brief Remove all clips
     */
    void clearClips() {
        m_clips.clear();
        clipsCleared.fire();
    }
    
    // ========== Clip Lookup ==========
    
    /**
     * @brief Get clip by ID
     */
    [[nodiscard]] ClipPtr getClip(const UUID& clipId) const {
        auto it = std::find_if(m_clips.begin(), m_clips.end(),
            [&clipId](const ClipPtr& c) { return c->id() == clipId; });
        return (it != m_clips.end()) ? *it : nullptr;
    }
    
    /**
     * @brief Get clip at timeline position
     */
    [[nodiscard]] ClipPtr getClipAt(Timestamp time) const {
        for (const auto& clip : m_clips) {
            if (clip->containsTime(time)) {
                return clip;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get clips that overlap a time range
     */
    [[nodiscard]] std::vector<ClipPtr> getClipsInRange(
        Timestamp start, Timestamp end) const 
    {
        std::vector<ClipPtr> result;
        for (const auto& clip : m_clips) {
            if (clip->timelineOut() > start && clip->timelineIn() < end) {
                result.push_back(clip);
            }
        }
        return result;
    }
    
    /**
     * @brief Get all clips (sorted by timeline position)
     */
    [[nodiscard]] const std::vector<ClipPtr>& clips() const {
        return m_clips;
    }
    
    /**
     * @brief Get number of clips
     */
    [[nodiscard]] size_t clipCount() const {
        return m_clips.size();
    }
    
    /**
     * @brief Check if track is empty
     */
    [[nodiscard]] bool empty() const {
        return m_clips.empty();
    }
    
    // ========== Time Range ==========
    
    /**
     * @brief Get track duration (end of last clip)
     */
    [[nodiscard]] Duration duration() const {
        if (m_clips.empty()) return 0;
        return m_clips.back()->timelineOut();
    }
    
    /**
     * @brief Check if a time range overlaps with existing clips
     * 
     * @param start Start time
     * @param end End time
     * @param excludeClip Clip to exclude from check (for move operations)
     */
    [[nodiscard]] bool hasOverlap(
        Timestamp start, Timestamp end, 
        const Clip* excludeClip) const 
    {
        for (const auto& clip : m_clips) {
            if (excludeClip && clip.get() == excludeClip) continue;
            
            // Check for overlap: !(end1 <= start2 || end2 <= start1)
            if (!(end <= clip->timelineIn() || clip->timelineOut() <= start)) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Find next gap after a position
     * 
     * @param afterTime Position to search from
     * @param minDuration Minimum gap duration
     * @return Start time of gap, or nullopt if no gap found
     */
    [[nodiscard]] std::optional<Timestamp> findGap(
        Timestamp afterTime, Duration minDuration) const 
    {
        if (m_clips.empty()) {
            return afterTime;
        }
        
        // Check gap before first clip
        if (m_clips.front()->timelineIn() >= afterTime + minDuration) {
            return afterTime;
        }
        
        // Check gaps between clips
        for (size_t i = 0; i < m_clips.size() - 1; ++i) {
            Timestamp gapStart = m_clips[i]->timelineOut();
            Timestamp gapEnd = m_clips[i + 1]->timelineIn();
            
            if (gapStart >= afterTime && gapEnd - gapStart >= minDuration) {
                return gapStart;
            }
        }
        
        // Return end of last clip
        return m_clips.back()->timelineOut();
    }
    
    // ========== Clip Operations ==========
    
    /**
     * @brief Move a clip to a new position
     * 
     * @param clipId Clip to move
     * @param newTimelineIn New start position
     * @return true if moved successfully
     */
    bool moveClip(const UUID& clipId, Timestamp newTimelineIn) {
        auto clip = getClip(clipId);
        if (!clip) return false;
        
        Duration dur = clip->duration();
        Timestamp newTimelineOut = newTimelineIn + dur;
        
        // Check for overlaps (excluding this clip)
        if (hasOverlap(newTimelineIn, newTimelineOut, clip.get())) {
            return false;
        }
        
        clip->setTimelineIn(newTimelineIn);
        clip->setTimelineOut(newTimelineOut);
        sortClips();
        
        clipMoved.fire(clip);
        return true;
    }
    
    // ========== Signals ==========
    
    Signal<ClipPtr> clipAdded;
    Signal<UUID> clipRemoved;
    Signal<ClipPtr> clipMoved;
    VoidSignal clipsCleared;
    
private:
    void sortClips() {
        std::sort(m_clips.begin(), m_clips.end(),
            [](const ClipPtr& a, const ClipPtr& b) {
                return a->timelineIn() < b->timelineIn();
            });
    }
    
    UUID m_id;
    std::string m_name;
    TrackType m_type = TrackType::Video;
    int m_index = -1;
    
    bool m_muted = false;
    bool m_locked = false;
    bool m_hidden = false;
    bool m_solo = false;
    
    std::vector<ClipPtr> m_clips;
};

} // namespace phoenix::model
