/**
 * @file timeline_controller.cpp
 * @brief Timeline controller implementation
 */

#include "timeline_controller.hpp"
#include "project_controller.hpp"

#include <phoenix/model/project.hpp>
#include <phoenix/model/sequence.hpp>
#include <phoenix/model/track.hpp>
#include <phoenix/model/clip.hpp>
#include <phoenix/model/media_item.hpp>
#include <phoenix/model/media_bin.hpp>
#include <phoenix/model/commands/undo_stack.hpp>
#include <phoenix/model/commands/clip_commands.hpp>

#include <algorithm>

namespace phoenix::editor {

TimelineController::TimelineController(ProjectController* projectController,
                                       QObject* parent)
    : QObject(parent)
    , m_projectController(projectController)
{
    // Connect to project changes
    connect(m_projectController, &ProjectController::projectChanged,
            this, &TimelineController::updateTracks);
    
    updateTracks();
}

TimelineController::~TimelineController() = default;

// ============================================================================
// Clip Operations
// ============================================================================

void TimelineController::addClip(const QString& mediaItemId,
                                 const QString& trackId,
                                 qint64 position) {
    auto* seq = sequence();
    if (!seq) return;
    
    // Find media item
    auto* project = m_projectController->project();
    if (!project) return;
    
    UUID mediaUuid = UUID::fromString(mediaItemId.toStdString());
    auto mediaItem = project->mediaBin().getItem(mediaUuid);
    if (!mediaItem) return;
    
    // Create clip
    auto clip = std::make_shared<model::Clip>(mediaUuid);
    clip->setTimelineIn(position);
    clip->setTimelineOut(position + mediaItem->duration());
    clip->setSourceIn(0);
    clip->setSourceOut(mediaItem->duration());
    clip->setName(mediaItem->name());
    
    // Execute command
    UUID trackUuid = UUID::fromString(trackId.toStdString());
    auto cmd = std::make_unique<model::AddClipCommand>(*seq, trackUuid, clip);
    undoStack()->push(std::move(cmd));
    
    updateTracks();
    emit clipAdded(QString::fromStdString(clip->id().toString()));
}

void TimelineController::addClipAtPlayhead(const QString& mediaItemId,
                                           const QString& trackId) {
    addClip(mediaItemId, trackId, playheadPosition());
}

void TimelineController::moveClip(const QString& clipId,
                                  const QString& targetTrackId,
                                  qint64 newPosition) {
    auto* seq = sequence();
    if (!seq) return;
    
    QString sourceTrackId = findTrackForClip(clipId);
    if (sourceTrackId.isEmpty()) return;
    
    UUID clipUuid = UUID::fromString(clipId.toStdString());
    UUID sourceUuid = UUID::fromString(sourceTrackId.toStdString());
    UUID targetUuid = UUID::fromString(targetTrackId.toStdString());
    
    auto cmd = std::make_unique<model::MoveClipCommand>(
        *seq, sourceUuid, clipUuid, targetUuid, newPosition);
    undoStack()->push(std::move(cmd));
    
    updateTracks();
    emit clipMoved(clipId);
}

void TimelineController::trimClipStart(const QString& clipId, qint64 newIn) {
    auto* seq = sequence();
    if (!seq) return;
    
    QString trackId = findTrackForClip(clipId);
    if (trackId.isEmpty()) return;
    
    UUID clipUuid = UUID::fromString(clipId.toStdString());
    UUID trackUuid = UUID::fromString(trackId.toStdString());
    
    auto cmd = std::make_unique<model::TrimClipCommand>(
        *seq, trackUuid, clipUuid, 
        model::TrimClipCommand::Edge::Start, newIn);
    undoStack()->push(std::move(cmd));
    
    updateTracks();
}

void TimelineController::trimClipEnd(const QString& clipId, qint64 newOut) {
    auto* seq = sequence();
    if (!seq) return;
    
    QString trackId = findTrackForClip(clipId);
    if (trackId.isEmpty()) return;
    
    UUID clipUuid = UUID::fromString(clipId.toStdString());
    UUID trackUuid = UUID::fromString(trackId.toStdString());
    
    auto cmd = std::make_unique<model::TrimClipCommand>(
        *seq, trackUuid, clipUuid,
        model::TrimClipCommand::Edge::End, newOut);
    undoStack()->push(std::move(cmd));
    
    updateTracks();
}

void TimelineController::splitClipAtPlayhead(const QString& clipId) {
    auto* seq = sequence();
    if (!seq) return;
    
    QString trackId = findTrackForClip(clipId);
    if (trackId.isEmpty()) return;
    
    UUID clipUuid = UUID::fromString(clipId.toStdString());
    UUID trackUuid = UUID::fromString(trackId.toStdString());
    
    auto cmd = std::make_unique<model::SplitClipCommand>(
        *seq, trackUuid, clipUuid, playheadPosition());
    undoStack()->push(std::move(cmd));
    
    updateTracks();
}

void TimelineController::deleteSelectedClip() {
    if (!m_selectedClipId.isEmpty()) {
        deleteClip(m_selectedClipId);
    }
}

void TimelineController::deleteClip(const QString& clipId) {
    auto* seq = sequence();
    if (!seq) return;
    
    QString trackId = findTrackForClip(clipId);
    if (trackId.isEmpty()) return;
    
    UUID clipUuid = UUID::fromString(clipId.toStdString());
    UUID trackUuid = UUID::fromString(trackId.toStdString());
    
    auto cmd = std::make_unique<model::DeleteClipCommand>(*seq, trackUuid, clipUuid);
    undoStack()->push(std::move(cmd));
    
    if (m_selectedClipId == clipId) {
        m_selectedClipId.clear();
        emit selectionChanged();
    }
    
    updateTracks();
    emit clipRemoved(clipId);
}

// ============================================================================
// Track Operations
// ============================================================================

void TimelineController::addVideoTrack() {
    auto* seq = sequence();
    if (!seq) return;
    
    seq->addVideoTrack();
    updateTracks();
}

void TimelineController::addAudioTrack() {
    auto* seq = sequence();
    if (!seq) return;
    
    seq->addAudioTrack();
    updateTracks();
}

void TimelineController::deleteTrack(const QString& trackId) {
    auto* seq = sequence();
    if (!seq) return;
    
    UUID uuid = UUID::fromString(trackId.toStdString());
    seq->removeTrack(uuid);
    updateTracks();
}

void TimelineController::setTrackMuted(const QString& trackId, bool muted) {
    auto* seq = sequence();
    if (!seq) return;
    
    UUID uuid = UUID::fromString(trackId.toStdString());
    if (auto track = seq->getTrack(uuid)) {
        track->setMuted(muted);
        updateTracks();
    }
}

void TimelineController::setTrackHidden(const QString& trackId, bool hidden) {
    auto* seq = sequence();
    if (!seq) return;
    
    UUID uuid = UUID::fromString(trackId.toStdString());
    if (auto track = seq->getTrack(uuid)) {
        track->setHidden(hidden);
        updateTracks();
    }
}

void TimelineController::setTrackLocked(const QString& trackId, bool locked) {
    auto* seq = sequence();
    if (!seq) return;
    
    UUID uuid = UUID::fromString(trackId.toStdString());
    if (auto track = seq->getTrack(uuid)) {
        track->setLocked(locked);
        updateTracks();
    }
}

// ============================================================================
// Selection
// ============================================================================

void TimelineController::selectClip(const QString& clipId) {
    if (m_selectedClipId != clipId) {
        m_selectedClipId = clipId;
        emit selectionChanged();
    }
}

void TimelineController::selectTrack(const QString& trackId) {
    if (m_selectedTrackId != trackId) {
        m_selectedTrackId = trackId;
        emit selectionChanged();
    }
}

void TimelineController::clearSelection() {
    m_selectedClipId.clear();
    m_selectedTrackId.clear();
    emit selectionChanged();
}

// ============================================================================
// Navigation
// ============================================================================

void TimelineController::goToStart() {
    setPlayheadPosition(0);
}

void TimelineController::goToEnd() {
    setPlayheadPosition(duration());
}

void TimelineController::goToNextClip() {
    auto* seq = sequence();
    if (!seq) return;
    
    qint64 currentPos = playheadPosition();
    qint64 nextPos = duration();
    
    // Find next clip boundary
    for (const auto& track : seq->videoTracks()) {
        for (const auto& clip : track->clips()) {
            if (clip->timelineIn() > currentPos) {
                nextPos = std::min(nextPos, static_cast<qint64>(clip->timelineIn()));
            }
            if (clip->timelineOut() > currentPos) {
                nextPos = std::min(nextPos, static_cast<qint64>(clip->timelineOut()));
            }
        }
    }
    
    setPlayheadPosition(nextPos);
}

void TimelineController::goToPrevClip() {
    auto* seq = sequence();
    if (!seq) return;
    
    qint64 currentPos = playheadPosition();
    qint64 prevPos = 0;
    
    // Find previous clip boundary
    for (const auto& track : seq->videoTracks()) {
        for (const auto& clip : track->clips()) {
            if (clip->timelineIn() < currentPos) {
                prevPos = std::max(prevPos, static_cast<qint64>(clip->timelineIn()));
            }
            if (clip->timelineOut() < currentPos) {
                prevPos = std::max(prevPos, static_cast<qint64>(clip->timelineOut()));
            }
        }
    }
    
    setPlayheadPosition(prevPos);
}

// ============================================================================
// Zoom
// ============================================================================

void TimelineController::zoomIn() {
    setPixelsPerSecond(m_pixelsPerSecond * 1.5);
}

void TimelineController::zoomOut() {
    setPixelsPerSecond(m_pixelsPerSecond / 1.5);
}

void TimelineController::zoomToFit() {
    // Calculate pixels per second to fit entire timeline
    qint64 dur = duration();
    if (dur <= 0) return;
    
    // Assume timeline view width of 1000 pixels
    double viewWidth = 1000.0;
    double durationSec = dur / 1'000'000.0;
    setPixelsPerSecond(viewWidth / durationSec);
}

// ============================================================================
// Time Conversion
// ============================================================================

double TimelineController::timeToPixels(qint64 timeUs) const {
    return (timeUs / 1'000'000.0) * m_pixelsPerSecond;
}

qint64 TimelineController::pixelsToTime(double pixels) const {
    return static_cast<qint64>((pixels / m_pixelsPerSecond) * 1'000'000.0);
}

QString TimelineController::formatTime(qint64 timeUs) const {
    qint64 totalSeconds = timeUs / 1'000'000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    // Calculate frames (assuming 30 fps for now)
    double fps = m_projectController->frameRate();
    int frames = static_cast<int>((timeUs % 1'000'000) * fps / 1'000'000.0);
    
    return QString("%1:%2:%3:%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(frames, 2, 10, QChar('0'));
}

// ============================================================================
// Property Getters/Setters
// ============================================================================

qint64 TimelineController::duration() const {
    auto* seq = sequence();
    return seq ? seq->duration() : 0;
}

qint64 TimelineController::playheadPosition() const {
    auto* seq = sequence();
    return seq ? seq->playheadPosition() : 0;
}

void TimelineController::setPlayheadPosition(qint64 pos) {
    auto* seq = sequence();
    if (!seq) return;
    
    pos = std::clamp(pos, qint64(0), duration());
    if (seq->playheadPosition() != pos) {
        seq->setPlayheadPosition(pos);
        emit playheadChanged();
    }
}

double TimelineController::pixelsPerSecond() const {
    return m_pixelsPerSecond;
}

void TimelineController::setPixelsPerSecond(double pps) {
    pps = std::clamp(pps, 10.0, 1000.0);
    if (m_pixelsPerSecond != pps) {
        m_pixelsPerSecond = pps;
        emit zoomChanged();
    }
}

QVariantList TimelineController::videoTracks() const {
    return m_videoTracks;
}

QVariantList TimelineController::audioTracks() const {
    return m_audioTracks;
}

int TimelineController::videoTrackCount() const {
    auto* seq = sequence();
    return seq ? static_cast<int>(seq->videoTrackCount()) : 0;
}

int TimelineController::audioTrackCount() const {
    auto* seq = sequence();
    return seq ? static_cast<int>(seq->audioTrackCount()) : 0;
}

QString TimelineController::selectedClipId() const {
    return m_selectedClipId;
}

QString TimelineController::selectedTrackId() const {
    return m_selectedTrackId;
}

bool TimelineController::snapEnabled() const {
    return m_snapEnabled;
}

void TimelineController::setSnapEnabled(bool enabled) {
    if (m_snapEnabled != enabled) {
        m_snapEnabled = enabled;
        emit snapChanged();
    }
}

// ============================================================================
// Private Methods
// ============================================================================

model::Sequence* TimelineController::sequence() const {
    auto* project = m_projectController->project();
    if (!project) return nullptr;
    auto seq = project->activeSequence();
    return seq.get();
}

model::UndoStack* TimelineController::undoStack() const {
    return m_projectController->undoStack();
}

QString TimelineController::findTrackForClip(const QString& clipId) const {
    auto* seq = sequence();
    if (!seq) return QString();
    
    UUID clipUuid = UUID::fromString(clipId.toStdString());
    
    for (const auto& track : seq->videoTracks()) {
        if (track->getClip(clipUuid)) {
            return QString::fromStdString(track->id().toString());
        }
    }
    
    for (const auto& track : seq->audioTracks()) {
        if (track->getClip(clipUuid)) {
            return QString::fromStdString(track->id().toString());
        }
    }
    
    return QString();
}

void TimelineController::updateTracks() {
    m_videoTracks.clear();
    m_audioTracks.clear();
    
    auto* seq = sequence();
    if (!seq) {
        emit tracksChanged();
        emit durationChanged();
        return;
    }
    
    // Build video tracks
    for (const auto& track : seq->videoTracks()) {
        QVariantMap trackMap;
        trackMap["id"] = QString::fromStdString(track->id().toString());
        trackMap["name"] = QString::fromStdString(track->name());
        trackMap["index"] = track->index();
        trackMap["muted"] = track->muted();
        trackMap["hidden"] = track->hidden();
        trackMap["locked"] = track->locked();
        trackMap["type"] = "video";
        
        // Build clips
        QVariantList clips;
        for (const auto& clip : track->clips()) {
            QVariantMap clipMap;
            clipMap["id"] = QString::fromStdString(clip->id().toString());
            clipMap["name"] = QString::fromStdString(clip->name());
            clipMap["timelineIn"] = static_cast<qint64>(clip->timelineIn());
            clipMap["timelineOut"] = static_cast<qint64>(clip->timelineOut());
            clipMap["duration"] = static_cast<qint64>(clip->duration());
            clipMap["sourceIn"] = static_cast<qint64>(clip->sourceIn());
            clipMap["sourceOut"] = static_cast<qint64>(clip->sourceOut());
            clipMap["selected"] = (m_selectedClipId == 
                QString::fromStdString(clip->id().toString()));
            clips.append(clipMap);
        }
        trackMap["clips"] = clips;
        
        m_videoTracks.append(trackMap);
    }
    
    // Build audio tracks (similar)
    for (const auto& track : seq->audioTracks()) {
        QVariantMap trackMap;
        trackMap["id"] = QString::fromStdString(track->id().toString());
        trackMap["name"] = QString::fromStdString(track->name());
        trackMap["index"] = track->index();
        trackMap["muted"] = track->muted();
        trackMap["locked"] = track->locked();
        trackMap["type"] = "audio";
        
        QVariantList clips;
        for (const auto& clip : track->clips()) {
            QVariantMap clipMap;
            clipMap["id"] = QString::fromStdString(clip->id().toString());
            clipMap["name"] = QString::fromStdString(clip->name());
            clipMap["timelineIn"] = static_cast<qint64>(clip->timelineIn());
            clipMap["timelineOut"] = static_cast<qint64>(clip->timelineOut());
            clipMap["duration"] = static_cast<qint64>(clip->duration());
            clipMap["selected"] = (m_selectedClipId == 
                QString::fromStdString(clip->id().toString()));
            clips.append(clipMap);
        }
        trackMap["clips"] = clips;
        
        m_audioTracks.append(trackMap);
    }
    
    emit tracksChanged();
    emit durationChanged();
}

} // namespace phoenix::editor
