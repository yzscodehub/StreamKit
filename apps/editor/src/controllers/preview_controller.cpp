/**
 * @file preview_controller.cpp
 * @brief Preview controller implementation
 */

#include "preview_controller.hpp"
#include "project_controller.hpp"
#include "timeline_controller.hpp"

#include <phoenix/model/project.hpp>
#include <phoenix/model/sequence.hpp>
#include <phoenix/engine/playback_engine.hpp>
#include <phoenix/engine/compositor.hpp>
#include <phoenix/media/decoder_pool.hpp>
#include <phoenix/media/frame.hpp>

#include <QMutexLocker>

namespace phoenix::editor {

// ============================================================================
// PreviewImageProvider
// ============================================================================

PreviewImageProvider::PreviewImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{}

QImage PreviewImageProvider::requestImage(const QString& id, QSize* size,
                                          const QSize& requestedSize) {
    Q_UNUSED(id)
    
    QMutexLocker locker(&m_mutex);
    
    QImage result = m_currentFrame;
    
    if (size) {
        *size = result.size();
    }
    
    if (requestedSize.isValid() && !result.isNull()) {
        result = result.scaled(requestedSize, Qt::KeepAspectRatio, 
                               Qt::SmoothTransformation);
    }
    
    return result;
}

void PreviewImageProvider::setCurrentFrame(const QImage& image) {
    QMutexLocker locker(&m_mutex);
    m_currentFrame = image;
}

// ============================================================================
// PreviewController
// ============================================================================

PreviewController::PreviewController(ProjectController* projectController,
                                     TimelineController* timelineController,
                                     QObject* parent)
    : QObject(parent)
    , m_projectController(projectController)
    , m_timelineController(timelineController)
    , m_imageProvider(new PreviewImageProvider())
{
    setupEngine();
    
    // Connect to timeline playhead changes
    connect(m_timelineController, &TimelineController::playheadChanged,
            this, &PreviewController::onPlayheadChanged);
    
    // Connect to project changes
    connect(m_projectController, &ProjectController::projectChanged,
            this, [this]() {
                stop();
                setupEngine();
                emit durationChanged();
                emit previewSizeChanged();
            });
}

PreviewController::~PreviewController() = default;

void PreviewController::setupEngine() {
    auto* project = m_projectController->project();
    if (!project) return;
    
    auto sequence = project->activeSequence();
    if (!sequence) return;
    
    // Create decoder pool
    m_decoderPool = std::make_unique<media::DecoderPool>();
    
    // Create compositor
    int width = sequence->settings().resolution.width;
    int height = sequence->settings().resolution.height;
    m_compositor = std::make_unique<engine::Compositor>(width, height);
    m_compositor->setSequence(sequence.get());
    
    // Set up frame decoder callback
    m_compositor->setFrameDecoder([this](const engine::FrameRequest& request) 
        -> std::shared_ptr<media::VideoFrame> {
        auto* project = m_projectController->project();
        if (!project) return nullptr;
        
        auto mediaItem = project->mediaBin().getItem(request.mediaItemId);
        if (!mediaItem) return nullptr;
        
        // Use convenience method that handles acquire/release
        auto result = m_decoderPool->decodeFrame(
            mediaItem->path(), request.mediaTime);
        
        if (!result) return nullptr;
        return std::make_shared<media::VideoFrame>(std::move(result.value()));
    });
    
    // Create playback engine
    m_playbackEngine = std::make_unique<engine::PlaybackEngine>();
    m_playbackEngine->setSequence(sequence.get());
    m_playbackEngine->setCompositor(m_compositor.get());
    
    // Connect frame callback
    m_playbackEngine->onFrame([this](auto frame, auto pts) {
        Q_UNUSED(pts)
        if (frame) {
            m_currentFrame = frameToImage(frame);
            m_imageProvider->setCurrentFrame(m_currentFrame);
            emit frameChanged();
        }
    });
    
    // Connect signals (ignore returned Connection, engine lifetime manages connections)
    (void)m_playbackEngine->stateChanged.connect([this](engine::PlaybackState state) {
        emit playbackStateChanged();
        
        switch (state) {
            case engine::PlaybackState::Playing:
                emit playbackStarted();
                break;
            case engine::PlaybackState::Paused:
                emit playbackPaused();
                break;
            case engine::PlaybackState::Stopped:
                emit playbackStopped();
                break;
            default:
                break;
        }
    });
    
    (void)m_playbackEngine->positionChanged.connect([this](Timestamp pos) {
        m_timelineController->setPlayheadPosition(pos);
        emit positionChanged();
    });
    
    // Render initial frame
    renderCurrentFrame();
}

// ============================================================================
// Playback Control
// ============================================================================

void PreviewController::play() {
    if (m_playbackEngine) {
        m_playbackEngine->setPlaybackSpeed(m_playbackSpeed);
        m_playbackEngine->setLooping(m_looping);
        m_playbackEngine->play();
    }
}

void PreviewController::pause() {
    if (m_playbackEngine) {
        m_playbackEngine->pause();
    }
}

void PreviewController::stop() {
    if (m_playbackEngine) {
        m_playbackEngine->stop();
    }
}

void PreviewController::togglePlayPause() {
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

// ============================================================================
// Navigation
// ============================================================================

void PreviewController::stepForward() {
    if (m_playbackEngine) {
        m_playbackEngine->stepForward();
        renderCurrentFrame();
    }
}

void PreviewController::stepBackward() {
    if (m_playbackEngine) {
        m_playbackEngine->stepBackward();
        renderCurrentFrame();
    }
}

void PreviewController::goToStart() {
    if (m_playbackEngine) {
        m_playbackEngine->goToStart();
        renderCurrentFrame();
    }
}

void PreviewController::goToEnd() {
    if (m_playbackEngine) {
        m_playbackEngine->goToEnd();
        renderCurrentFrame();
    }
}

void PreviewController::seek(qint64 position) {
    if (m_playbackEngine) {
        m_playbackEngine->seek(position);
        renderCurrentFrame();
    }
}

void PreviewController::seekToProgress(double progress) {
    seek(static_cast<qint64>(progress * duration()));
}

// ============================================================================
// Property Getters/Setters
// ============================================================================

bool PreviewController::isPlaying() const {
    return m_playbackEngine && m_playbackEngine->isPlaying();
}

bool PreviewController::isPaused() const {
    return m_playbackEngine && m_playbackEngine->isPaused();
}

bool PreviewController::isStopped() const {
    return m_playbackEngine && m_playbackEngine->isStopped();
}

qint64 PreviewController::position() const {
    return m_playbackEngine ? m_playbackEngine->currentTime() : 0;
}

qint64 PreviewController::duration() const {
    return m_playbackEngine ? m_playbackEngine->duration() : 0;
}

double PreviewController::progress() const {
    qint64 dur = duration();
    return dur > 0 ? static_cast<double>(position()) / dur : 0.0;
}

double PreviewController::playbackSpeed() const {
    return m_playbackSpeed;
}

void PreviewController::setPlaybackSpeed(double speed) {
    m_playbackSpeed = std::clamp(speed, 0.25, 4.0);
    if (m_playbackEngine) {
        m_playbackEngine->setPlaybackSpeed(m_playbackSpeed);
    }
    emit playbackSpeedChanged();
}

bool PreviewController::looping() const {
    return m_looping;
}

void PreviewController::setLooping(bool loop) {
    m_looping = loop;
    if (m_playbackEngine) {
        m_playbackEngine->setLooping(loop);
    }
    emit loopingChanged();
}

int PreviewController::previewWidth() const {
    return m_projectController->frameWidth();
}

int PreviewController::previewHeight() const {
    return m_projectController->frameHeight();
}

QString PreviewController::frameInfo() const {
    return QString("%1 / %2")
        .arg(m_timelineController->formatTime(position()))
        .arg(m_timelineController->formatTime(duration()));
}

// ============================================================================
// Private Slots
// ============================================================================

void PreviewController::onFrameReady() {
    emit frameChanged();
}

void PreviewController::onPlayheadChanged() {
    if (!isPlaying()) {
        renderCurrentFrame();
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void PreviewController::renderCurrentFrame() {
    if (!m_compositor) return;
    
    qint64 time = m_timelineController->playheadPosition();
    auto result = m_compositor->compose(time);
    
    if (result.frame) {
        m_currentFrame = frameToImage(result.frame);
        m_imageProvider->setCurrentFrame(m_currentFrame);
        emit frameChanged();
    }
}

QImage PreviewController::frameToImage(
    const std::shared_ptr<media::VideoFrame>& frame) 
{
    if (!frame || !frame->isValid()) {
        return QImage();
    }
    
    // Transfer to CPU if hardware frame
    media::VideoFrame cpuFrame;
    if (frame->isHardwareFrame()) {
        auto result = frame->transferToCPU();
        if (!result) {
            return QImage();
        }
        cpuFrame = std::move(result.value());
    } else {
        cpuFrame = *frame;
    }
    
    int width = cpuFrame.width();
    int height = cpuFrame.height();
    
    // Create QImage from frame data
    // Assuming RGBA format for now
    QImage image(width, height, QImage::Format_RGBA8888);
    
    const uint8_t* src = cpuFrame.data(0);
    int srcStride = cpuFrame.linesize(0);
    
    for (int y = 0; y < height; ++y) {
        memcpy(image.scanLine(y), src + y * srcStride, width * 4);
    }
    
    return image;
}

} // namespace phoenix::editor
