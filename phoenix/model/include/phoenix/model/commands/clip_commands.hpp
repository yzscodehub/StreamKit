/**
 * @file clip_commands.hpp
 * @brief Commands for clip manipulation
 * 
 * Commands for adding, moving, trimming, and deleting clips.
 */

#pragma once

#include <phoenix/model/commands/command.hpp>
#include <phoenix/model/clip.hpp>
#include <phoenix/model/track.hpp>
#include <phoenix/model/sequence.hpp>
#include <phoenix/core/types.hpp>

#include <memory>
#include <optional>

namespace phoenix::model {

/**
 * @brief Command IDs for merging
 */
enum class CommandId : int {
    None = -1,
    MoveClip = 1,
    TrimClipStart = 2,
    TrimClipEnd = 3,
};

// ============================================================================
// AddClipCommand
// ============================================================================

/**
 * @brief Add a clip to a track
 */
class AddClipCommand : public Command {
public:
    AddClipCommand(Sequence& sequence, const UUID& trackId, std::shared_ptr<Clip> clip)
        : m_sequence(sequence)
        , m_trackId(trackId)
        , m_clip(std::move(clip))
        , m_clipId(m_clip->id()) {}
    
    void execute() override {
        if (auto track = m_sequence.getTrack(m_trackId)) {
            track->addClip(m_clip);
        }
    }
    
    void undo() override {
        if (auto track = m_sequence.getTrack(m_trackId)) {
            track->removeClip(m_clipId);
        }
    }
    
    [[nodiscard]] std::string description() const override {
        return "Add Clip";
    }
    
private:
    Sequence& m_sequence;
    UUID m_trackId;
    std::shared_ptr<Clip> m_clip;
    UUID m_clipId;
};

// ============================================================================
// DeleteClipCommand
// ============================================================================

/**
 * @brief Delete a clip from a track
 */
class DeleteClipCommand : public Command {
public:
    DeleteClipCommand(Sequence& sequence, const UUID& trackId, const UUID& clipId)
        : m_sequence(sequence)
        , m_trackId(trackId)
        , m_clipId(clipId) {}
    
    void execute() override {
        if (auto track = m_sequence.getTrack(m_trackId)) {
            // Save clip for undo
            m_savedClip = track->getClip(m_clipId);
            track->removeClip(m_clipId);
        }
    }
    
    void undo() override {
        if (m_savedClip) {
            if (auto track = m_sequence.getTrack(m_trackId)) {
                track->addClip(m_savedClip);
            }
        }
    }
    
    [[nodiscard]] std::string description() const override {
        return "Delete Clip";
    }
    
private:
    Sequence& m_sequence;
    UUID m_trackId;
    UUID m_clipId;
    std::shared_ptr<Clip> m_savedClip;
};

// ============================================================================
// MoveClipCommand
// ============================================================================

/**
 * @brief Move a clip to a new position (same or different track)
 */
class MoveClipCommand : public Command {
public:
    MoveClipCommand(Sequence& sequence,
                    const UUID& sourceTrackId,
                    const UUID& clipId,
                    const UUID& destTrackId,
                    Timestamp newPosition)
        : m_sequence(sequence)
        , m_sourceTrackId(sourceTrackId)
        , m_destTrackId(destTrackId)
        , m_clipId(clipId)
        , m_newPosition(newPosition) {}
    
    void execute() override {
        auto sourceTrack = m_sequence.getTrack(m_sourceTrackId);
        if (!sourceTrack) return;
        
        auto clip = sourceTrack->getClip(m_clipId);
        if (!clip) return;
        
        // Save old state
        m_oldPosition = clip->timelineIn();
        
        if (m_sourceTrackId == m_destTrackId) {
            // Same track - use moveClip which handles overlap check
            sourceTrack->moveClip(m_clipId, m_newPosition);
        } else {
            // Different track - remove from source, add to dest
            auto destTrack = m_sequence.getTrack(m_destTrackId);
            if (!destTrack) return;
            
            Duration dur = clip->duration();
            clip->setTimelineIn(m_newPosition);
            clip->setTimelineOut(m_newPosition + dur);
            
            sourceTrack->removeClip(m_clipId);
            destTrack->addClip(clip);
        }
    }
    
    void undo() override {
        auto currentTrack = m_sequence.getTrack(m_destTrackId);
        if (!currentTrack) return;
        
        auto clip = currentTrack->getClip(m_clipId);
        if (!clip) return;
        
        if (m_sourceTrackId == m_destTrackId) {
            // Same track - restore position
            currentTrack->moveClip(m_clipId, m_oldPosition);
        } else {
            // Different track - move back
            auto sourceTrack = m_sequence.getTrack(m_sourceTrackId);
            if (!sourceTrack) return;
            
            Duration dur = clip->duration();
            clip->setTimelineIn(m_oldPosition);
            clip->setTimelineOut(m_oldPosition + dur);
            
            currentTrack->removeClip(m_clipId);
            sourceTrack->addClip(clip);
        }
    }
    
    [[nodiscard]] std::string description() const override {
        return "Move Clip";
    }
    
    [[nodiscard]] int id() const override {
        return static_cast<int>(CommandId::MoveClip);
    }
    
    bool mergeWith(const Command* other) override {
        auto* moveCmd = dynamic_cast<const MoveClipCommand*>(other);
        if (!moveCmd) return false;
        if (moveCmd->m_clipId != m_clipId) return false;
        
        // Update destination
        m_destTrackId = moveCmd->m_destTrackId;
        m_newPosition = moveCmd->m_newPosition;
        return true;
    }
    
private:
    Sequence& m_sequence;
    UUID m_sourceTrackId;
    UUID m_destTrackId;
    UUID m_clipId;
    Timestamp m_newPosition;
    Timestamp m_oldPosition{0};
};

// ============================================================================
// TrimClipCommand
// ============================================================================

/**
 * @brief Trim clip's start or end
 */
class TrimClipCommand : public Command {
public:
    enum class Edge { Start, End };
    
    TrimClipCommand(Sequence& sequence,
                    const UUID& trackId,
                    const UUID& clipId,
                    Edge edge,
                    Timestamp newBoundary)
        : m_sequence(sequence)
        , m_trackId(trackId)
        , m_clipId(clipId)
        , m_edge(edge)
        , m_newBoundary(newBoundary) {}
    
    void execute() override {
        auto track = m_sequence.getTrack(m_trackId);
        if (!track) return;
        
        auto clip = track->getClip(m_clipId);
        if (!clip) return;
        
        if (m_edge == Edge::Start) {
            m_oldBoundary = clip->timelineIn();
            m_oldSourceIn = clip->sourceIn();
            
            // Calculate source offset change
            Duration delta = m_newBoundary - clip->timelineIn();
            
            clip->setTimelineIn(m_newBoundary);
            clip->setSourceIn(clip->sourceIn() + delta);
            // timelineOut stays the same, so duration shrinks
        } else {
            m_oldBoundary = clip->timelineOut();
            clip->setTimelineOut(m_newBoundary);
        }
    }
    
    void undo() override {
        auto track = m_sequence.getTrack(m_trackId);
        if (!track) return;
        
        auto clip = track->getClip(m_clipId);
        if (!clip) return;
        
        if (m_edge == Edge::Start) {
            clip->setTimelineIn(m_oldBoundary);
            clip->setSourceIn(m_oldSourceIn);
        } else {
            clip->setTimelineOut(m_oldBoundary);
        }
    }
    
    [[nodiscard]] std::string description() const override {
        return m_edge == Edge::Start ? "Trim Clip Start" : "Trim Clip End";
    }
    
    [[nodiscard]] int id() const override {
        return m_edge == Edge::Start 
            ? static_cast<int>(CommandId::TrimClipStart)
            : static_cast<int>(CommandId::TrimClipEnd);
    }
    
    bool mergeWith(const Command* other) override {
        auto* trimCmd = dynamic_cast<const TrimClipCommand*>(other);
        if (!trimCmd) return false;
        if (trimCmd->m_clipId != m_clipId) return false;
        if (trimCmd->m_edge != m_edge) return false;
        
        m_newBoundary = trimCmd->m_newBoundary;
        return true;
    }
    
private:
    Sequence& m_sequence;
    UUID m_trackId;
    UUID m_clipId;
    Edge m_edge;
    Timestamp m_newBoundary;
    Timestamp m_oldBoundary{0};
    Timestamp m_oldSourceIn{0};
};

// ============================================================================
// SplitClipCommand
// ============================================================================

/**
 * @brief Split a clip at a given position
 */
class SplitClipCommand : public Command {
public:
    SplitClipCommand(Sequence& sequence,
                     const UUID& trackId,
                     const UUID& clipId,
                     Timestamp splitPoint)
        : m_sequence(sequence)
        , m_trackId(trackId)
        , m_clipId(clipId)
        , m_splitPoint(splitPoint)
        , m_newClipId(UUID::generate()) {}
    
    void execute() override {
        auto track = m_sequence.getTrack(m_trackId);
        if (!track) return;
        
        auto clip = track->getClip(m_clipId);
        if (!clip) return;
        
        // Validate split point
        if (m_splitPoint <= clip->timelineIn() || 
            m_splitPoint >= clip->timelineOut()) {
            return;
        }
        
        // Save original state
        m_originalTimelineOut = clip->timelineOut();
        m_originalSourceOut = clip->sourceOut();
        
        // Calculate durations
        Duration firstDuration = m_splitPoint - clip->timelineIn();
        
        // Create second clip
        auto secondClip = std::make_shared<Clip>(clip->mediaItemId());
        secondClip->setTimelineIn(m_splitPoint);
        secondClip->setTimelineOut(m_originalTimelineOut);
        secondClip->setSourceIn(clip->sourceIn() + firstDuration);
        secondClip->setSourceOut(m_originalSourceOut);
        secondClip->setName(clip->name() + " (2)");
        secondClip->setOpacity(clip->opacity());
        secondClip->setVolume(clip->volume());
        m_newClipId = secondClip->id();
        
        // Trim first clip
        clip->setTimelineOut(m_splitPoint);
        clip->setSourceOut(clip->sourceIn() + firstDuration);
        
        // Add second clip
        track->addClip(secondClip);
        m_executed = true;
    }
    
    void undo() override {
        if (!m_executed) return;
        
        auto track = m_sequence.getTrack(m_trackId);
        if (!track) return;
        
        // Remove second clip
        track->removeClip(m_newClipId);
        
        // Restore first clip
        if (auto clip = track->getClip(m_clipId)) {
            clip->setTimelineOut(m_originalTimelineOut);
            clip->setSourceOut(m_originalSourceOut);
        }
        
        m_executed = false;
    }
    
    [[nodiscard]] std::string description() const override {
        return "Split Clip";
    }
    
    [[nodiscard]] const UUID& newClipId() const {
        return m_newClipId;
    }
    
private:
    Sequence& m_sequence;
    UUID m_trackId;
    UUID m_clipId;
    Timestamp m_splitPoint;
    UUID m_newClipId;
    Timestamp m_originalTimelineOut{0};
    Timestamp m_originalSourceOut{0};
    bool m_executed = false;
};

} // namespace phoenix::model
