/**
 * @file timeline_controller.hpp
 * @brief Timeline controller for QML
 * 
 * Handles timeline operations: clip manipulation,
 * track management, and playhead control.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QPointF>

namespace phoenix::model {
    class Sequence;
    class UndoStack;
}

namespace phoenix::editor {

class ProjectController;

/**
 * @brief Timeline controller for QML
 * 
 * Exposes timeline operations including:
 * - Add/move/trim/delete clips
 * - Track management
 * - Playhead position
 * - Selection
 */
class TimelineController : public QObject {
    Q_OBJECT
    
    // Timeline state
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 playheadPosition READ playheadPosition 
               WRITE setPlayheadPosition NOTIFY playheadChanged)
    Q_PROPERTY(double pixelsPerSecond READ pixelsPerSecond 
               WRITE setPixelsPerSecond NOTIFY zoomChanged)
    
    // Tracks
    Q_PROPERTY(QVariantList videoTracks READ videoTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(int videoTrackCount READ videoTrackCount NOTIFY tracksChanged)
    Q_PROPERTY(int audioTrackCount READ audioTrackCount NOTIFY tracksChanged)
    
    // Selection
    Q_PROPERTY(QString selectedClipId READ selectedClipId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTrackId READ selectedTrackId NOTIFY selectionChanged)
    
    // Snap
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapChanged)

public:
    explicit TimelineController(ProjectController* projectController, 
                                QObject* parent = nullptr);
    ~TimelineController() override;
    
    // ========== Clip Operations ==========
    
    /// Add clip from media item to track
    Q_INVOKABLE void addClip(const QString& mediaItemId, 
                             const QString& trackId,
                             qint64 position);
    
    /// Add clip at playhead position
    Q_INVOKABLE void addClipAtPlayhead(const QString& mediaItemId,
                                       const QString& trackId);
    
    /// Move clip to new position
    Q_INVOKABLE void moveClip(const QString& clipId,
                              const QString& targetTrackId,
                              qint64 newPosition);
    
    /// Trim clip start
    Q_INVOKABLE void trimClipStart(const QString& clipId, qint64 newIn);
    
    /// Trim clip end
    Q_INVOKABLE void trimClipEnd(const QString& clipId, qint64 newOut);
    
    /// Split clip at playhead
    Q_INVOKABLE void splitClipAtPlayhead(const QString& clipId);
    
    /// Delete selected clip
    Q_INVOKABLE void deleteSelectedClip();
    
    /// Delete clip by ID
    Q_INVOKABLE void deleteClip(const QString& clipId);
    
    // ========== Track Operations ==========
    
    Q_INVOKABLE void addVideoTrack();
    Q_INVOKABLE void addAudioTrack();
    Q_INVOKABLE void deleteTrack(const QString& trackId);
    Q_INVOKABLE void setTrackMuted(const QString& trackId, bool muted);
    Q_INVOKABLE void setTrackHidden(const QString& trackId, bool hidden);
    Q_INVOKABLE void setTrackLocked(const QString& trackId, bool locked);
    
    // ========== Selection ==========
    
    Q_INVOKABLE void selectClip(const QString& clipId);
    Q_INVOKABLE void selectTrack(const QString& trackId);
    Q_INVOKABLE void clearSelection();
    
    // ========== Navigation ==========
    
    Q_INVOKABLE void goToStart();
    Q_INVOKABLE void goToEnd();
    Q_INVOKABLE void goToNextClip();
    Q_INVOKABLE void goToPrevClip();
    
    // ========== Zoom ==========
    
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomToFit();
    
    // ========== Time Conversion ==========
    
    /// Convert timeline position to pixel X
    Q_INVOKABLE double timeToPixels(qint64 timeUs) const;
    
    /// Convert pixel X to timeline position
    Q_INVOKABLE qint64 pixelsToTime(double pixels) const;
    
    /// Format time as string (HH:MM:SS:FF)
    Q_INVOKABLE QString formatTime(qint64 timeUs) const;
    
    // ========== Property Getters ==========
    
    qint64 duration() const;
    qint64 playheadPosition() const;
    void setPlayheadPosition(qint64 pos);
    
    double pixelsPerSecond() const;
    void setPixelsPerSecond(double pps);
    
    QVariantList videoTracks() const;
    QVariantList audioTracks() const;
    int videoTrackCount() const;
    int audioTrackCount() const;
    
    QString selectedClipId() const;
    QString selectedTrackId() const;
    
    bool snapEnabled() const;
    void setSnapEnabled(bool enabled);

signals:
    void durationChanged();
    void playheadChanged();
    void zoomChanged();
    void tracksChanged();
    void selectionChanged();
    void snapChanged();
    
    void clipAdded(const QString& clipId);
    void clipRemoved(const QString& clipId);
    void clipMoved(const QString& clipId);

private:
    model::Sequence* sequence() const;
    model::UndoStack* undoStack() const;
    QString findTrackForClip(const QString& clipId) const;
    void updateTracks();
    
    ProjectController* m_projectController;
    
    double m_pixelsPerSecond = 100.0;  // Zoom level
    QString m_selectedClipId;
    QString m_selectedTrackId;
    bool m_snapEnabled = true;
    
    QVariantList m_videoTracks;
    QVariantList m_audioTracks;
};

} // namespace phoenix::editor
