/**
 * @file preview_controller.hpp
 * @brief Preview/playback controller for QML
 * 
 * Handles video preview, playback control, and
 * frame display.
 */

#pragma once

#include <QObject>
#include <QImage>
#include <QQuickImageProvider>
#include <QMutex>
#include <memory>

namespace phoenix::engine {
    class PlaybackEngine;
    class Compositor;
}

namespace phoenix::media {
    class DecoderPool;
    class VideoFrame;
}

namespace phoenix::editor {

class ProjectController;
class TimelineController;

/**
 * @brief Preview image provider for QML
 */
class PreviewImageProvider : public QQuickImageProvider {
public:
    PreviewImageProvider();
    
    QImage requestImage(const QString& id, QSize* size, 
                        const QSize& requestedSize) override;
    
    void setCurrentFrame(const QImage& image);
    
private:
    QImage m_currentFrame;
    mutable QMutex m_mutex;
};

/**
 * @brief Preview controller for QML
 * 
 * Manages playback and frame display including:
 * - Play/pause/stop
 * - Frame stepping
 * - Playback speed
 * - Loop mode
 */
class PreviewController : public QObject {
    Q_OBJECT
    
    // Playback state
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isStopped READ isStopped NOTIFY playbackStateChanged)
    
    // Position
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    
    // Settings
    Q_PROPERTY(double playbackSpeed READ playbackSpeed 
               WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(bool looping READ looping WRITE setLooping NOTIFY loopingChanged)
    
    // Preview size
    Q_PROPERTY(int previewWidth READ previewWidth NOTIFY previewSizeChanged)
    Q_PROPERTY(int previewHeight READ previewHeight NOTIFY previewSizeChanged)
    
    // Frame info
    Q_PROPERTY(QString frameInfo READ frameInfo NOTIFY frameChanged)

public:
    PreviewController(ProjectController* projectController,
                      TimelineController* timelineController,
                      QObject* parent = nullptr);
    ~PreviewController() override;
    
    // ========== Playback Control ==========
    
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePlayPause();
    
    // ========== Navigation ==========
    
    Q_INVOKABLE void stepForward();
    Q_INVOKABLE void stepBackward();
    Q_INVOKABLE void goToStart();
    Q_INVOKABLE void goToEnd();
    Q_INVOKABLE void seek(qint64 position);
    Q_INVOKABLE void seekToProgress(double progress);
    
    // ========== Property Getters ==========
    
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    
    qint64 position() const;
    qint64 duration() const;
    double progress() const;
    
    double playbackSpeed() const;
    void setPlaybackSpeed(double speed);
    
    bool looping() const;
    void setLooping(bool loop);
    
    int previewWidth() const;
    int previewHeight() const;
    
    QString frameInfo() const;
    
    // ========== Image Provider ==========
    
    PreviewImageProvider* imageProvider() const { return m_imageProvider; }

signals:
    void playbackStateChanged();
    void positionChanged();
    void durationChanged();
    void playbackSpeedChanged();
    void loopingChanged();
    void previewSizeChanged();
    void frameChanged();
    
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();

private slots:
    void onFrameReady();
    void onPlayheadChanged();

private:
    void setupEngine();
    void renderCurrentFrame();
    QImage frameToImage(const std::shared_ptr<media::VideoFrame>& frame);
    
    ProjectController* m_projectController;
    TimelineController* m_timelineController;
    
    std::unique_ptr<engine::PlaybackEngine> m_playbackEngine;
    std::unique_ptr<engine::Compositor> m_compositor;
    std::unique_ptr<media::DecoderPool> m_decoderPool;
    
    PreviewImageProvider* m_imageProvider;  // Owned by QML engine
    
    double m_playbackSpeed = 1.0;
    bool m_looping = false;
    QImage m_currentFrame;
};

} // namespace phoenix::editor
