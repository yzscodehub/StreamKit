/**
 * @file sequence.hpp
 * @brief Sequence (timeline) containing tracks
 * 
 * A Sequence is a complete timeline containing multiple video
 * and audio tracks. It represents an editable composition.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/uuid.hpp>
#include <phoenix/core/signals.hpp>
#include <phoenix/model/track.hpp>
#include <vector>
#include <memory>
#include <algorithm>

namespace phoenix::model {

/**
 * @brief Sequence settings
 */
struct SequenceSettings {
    Size resolution{1920, 1080};        // Output resolution
    Rational frameRate{30000, 1001};    // ~29.97 fps
    int sampleRate = 48000;             // Audio sample rate
    int audioChannels = 2;              // Stereo
    
    /// Frame duration in microseconds
    [[nodiscard]] Duration frameDuration() const {
        if (frameRate.num == 0) return 0;
        return static_cast<Duration>(
            static_cast<double>(kTimeBaseUs) * frameRate.den / frameRate.num
        );
    }
    
    /// Frames per second as double
    [[nodiscard]] double fps() const {
        return frameRate.toDouble();
    }
};

/**
 * @brief A sequence (timeline composition)
 * 
 * Contains multiple video and audio tracks. Video tracks are
 * composited from bottom (index 0) to top. Audio tracks are mixed.
 */
class Sequence {
public:
    using TrackPtr = std::shared_ptr<Track>;
    using ClipPtr = std::shared_ptr<Clip>;
    
    Sequence() : m_id(UUID::generate()) {
        // Create default tracks
        addVideoTrack();
        addAudioTrack();
    }
    
    explicit Sequence(const std::string& name)
        : m_id(UUID::generate())
        , m_name(name)
    {
        addVideoTrack();
        addAudioTrack();
    }
    
    // ========== Identification ==========
    
    [[nodiscard]] const UUID& id() const { return m_id; }
    
    [[nodiscard]] const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    // ========== Settings ==========
    
    [[nodiscard]] const SequenceSettings& settings() const { return m_settings; }
    SequenceSettings& settings() { return m_settings; }
    void setSettings(const SequenceSettings& settings) { m_settings = settings; }
    
    // ========== Track Management ==========
    
    /**
     * @brief Add a new video track
     * 
     * @param index Insert position (-1 = top)
     * @return Pointer to the new track
     */
    TrackPtr addVideoTrack(int index = -1) {
        auto track = std::make_shared<Track>(TrackType::Video);
        track->setName("Video " + std::to_string(m_videoTracks.size() + 1));
        
        if (index < 0 || index >= static_cast<int>(m_videoTracks.size())) {
            track->setIndex(static_cast<int>(m_videoTracks.size()));
            m_videoTracks.push_back(track);
        } else {
            m_videoTracks.insert(m_videoTracks.begin() + index, track);
            updateTrackIndices();
        }
        
        trackAdded.fire(track);
        return track;
    }
    
    /**
     * @brief Add a new audio track
     * 
     * @param index Insert position (-1 = bottom)
     * @return Pointer to the new track
     */
    TrackPtr addAudioTrack(int index = -1) {
        auto track = std::make_shared<Track>(TrackType::Audio);
        track->setName("Audio " + std::to_string(m_audioTracks.size() + 1));
        
        if (index < 0 || index >= static_cast<int>(m_audioTracks.size())) {
            track->setIndex(static_cast<int>(m_audioTracks.size()));
            m_audioTracks.push_back(track);
        } else {
            m_audioTracks.insert(m_audioTracks.begin() + index, track);
            updateAudioTrackIndices();
        }
        
        trackAdded.fire(track);
        return track;
    }
    
    /**
     * @brief Remove a track
     * 
     * @param trackId UUID of track to remove
     * @return true if removed
     */
    bool removeTrack(const UUID& trackId) {
        // Try video tracks
        auto vit = std::find_if(m_videoTracks.begin(), m_videoTracks.end(),
            [&trackId](const TrackPtr& t) { return t->id() == trackId; });
        
        if (vit != m_videoTracks.end()) {
            m_videoTracks.erase(vit);
            updateTrackIndices();
            trackRemoved.fire(trackId);
            return true;
        }
        
        // Try audio tracks
        auto ait = std::find_if(m_audioTracks.begin(), m_audioTracks.end(),
            [&trackId](const TrackPtr& t) { return t->id() == trackId; });
        
        if (ait != m_audioTracks.end()) {
            m_audioTracks.erase(ait);
            updateAudioTrackIndices();
            trackRemoved.fire(trackId);
            return true;
        }
        
        return false;
    }
    
    // ========== Track Access ==========
    
    [[nodiscard]] const std::vector<TrackPtr>& videoTracks() const {
        return m_videoTracks;
    }
    
    [[nodiscard]] const std::vector<TrackPtr>& audioTracks() const {
        return m_audioTracks;
    }
    
    [[nodiscard]] TrackPtr getVideoTrack(int index) const {
        if (index < 0 || index >= static_cast<int>(m_videoTracks.size())) {
            return nullptr;
        }
        return m_videoTracks[index];
    }
    
    [[nodiscard]] TrackPtr getAudioTrack(int index) const {
        if (index < 0 || index >= static_cast<int>(m_audioTracks.size())) {
            return nullptr;
        }
        return m_audioTracks[index];
    }
    
    [[nodiscard]] TrackPtr getTrack(const UUID& trackId) const {
        for (const auto& track : m_videoTracks) {
            if (track->id() == trackId) return track;
        }
        for (const auto& track : m_audioTracks) {
            if (track->id() == trackId) return track;
        }
        return nullptr;
    }
    
    [[nodiscard]] size_t videoTrackCount() const { return m_videoTracks.size(); }
    [[nodiscard]] size_t audioTrackCount() const { return m_audioTracks.size(); }
    
    // ========== Clip Access ==========
    
    /**
     * @brief Get clip by ID (searches all tracks)
     */
    [[nodiscard]] ClipPtr getClip(const UUID& clipId) const {
        for (const auto& track : m_videoTracks) {
            if (auto clip = track->getClip(clipId)) return clip;
        }
        for (const auto& track : m_audioTracks) {
            if (auto clip = track->getClip(clipId)) return clip;
        }
        return nullptr;
    }
    
    /**
     * @brief Get all clips at a specific time
     */
    [[nodiscard]] std::vector<ClipPtr> getClipsAt(Timestamp time) const {
        std::vector<ClipPtr> result;
        
        for (const auto& track : m_videoTracks) {
            if (!track->hidden() && !track->muted()) {
                if (auto clip = track->getClipAt(time)) {
                    result.push_back(clip);
                }
            }
        }
        for (const auto& track : m_audioTracks) {
            if (!track->muted()) {
                if (auto clip = track->getClipAt(time)) {
                    result.push_back(clip);
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get visible video clips at a time (for compositing)
     * 
     * Returns clips sorted from bottom to top (compositing order).
     */
    [[nodiscard]] std::vector<ClipPtr> getVisibleClipsAt(Timestamp time) const {
        std::vector<ClipPtr> result;
        
        for (const auto& track : m_videoTracks) {
            if (track->hidden() || track->muted()) continue;
            if (auto clip = track->getClipAt(time)) {
                if (!clip->disabled()) {
                    result.push_back(clip);
                }
            }
        }
        
        return result;
    }
    
    // ========== Duration ==========
    
    /**
     * @brief Get total duration (end of last clip)
     */
    [[nodiscard]] Duration duration() const {
        Duration maxDuration = 0;
        
        for (const auto& track : m_videoTracks) {
            maxDuration = std::max(maxDuration, track->duration());
        }
        for (const auto& track : m_audioTracks) {
            maxDuration = std::max(maxDuration, track->duration());
        }
        
        return maxDuration;
    }
    
    /**
     * @brief Get frame count
     */
    [[nodiscard]] int64_t frameCount() const {
        Duration dur = duration();
        Duration frameDur = m_settings.frameDuration();
        if (frameDur == 0) return 0;
        return dur / frameDur;
    }
    
    // ========== Playhead ==========
    
    [[nodiscard]] Timestamp playheadPosition() const { return m_playhead; }
    void setPlayheadPosition(Timestamp pos) { 
        m_playhead = pos;
        playheadMoved.fire(pos);
    }
    
    // ========== In/Out Points ==========
    
    [[nodiscard]] Timestamp inPoint() const { return m_inPoint; }
    void setInPoint(Timestamp pos) { m_inPoint = pos; }
    
    [[nodiscard]] Timestamp outPoint() const { return m_outPoint; }
    void setOutPoint(Timestamp pos) { m_outPoint = pos; }
    
    [[nodiscard]] bool hasInOutRange() const {
        return m_outPoint > m_inPoint;
    }
    
    [[nodiscard]] Duration inOutDuration() const {
        return hasInOutRange() ? m_outPoint - m_inPoint : duration();
    }
    
    // ========== Signals ==========
    
    Signal<TrackPtr> trackAdded;
    Signal<UUID> trackRemoved;
    Signal<Timestamp> playheadMoved;
    
private:
    void updateTrackIndices() {
        for (size_t i = 0; i < m_videoTracks.size(); ++i) {
            m_videoTracks[i]->setIndex(static_cast<int>(i));
        }
    }
    
    void updateAudioTrackIndices() {
        for (size_t i = 0; i < m_audioTracks.size(); ++i) {
            m_audioTracks[i]->setIndex(static_cast<int>(i));
        }
    }
    
    UUID m_id;
    std::string m_name{"Sequence 1"};
    SequenceSettings m_settings;
    
    std::vector<TrackPtr> m_videoTracks;
    std::vector<TrackPtr> m_audioTracks;
    
    Timestamp m_playhead = 0;
    Timestamp m_inPoint = 0;
    Timestamp m_outPoint = 0;
};

} // namespace phoenix::model
