/**
 * @file videocontroller.cpp
 * @brief VideoController implementation
 */

#include "videocontroller.hpp"
#include <spdlog/spdlog.h>

namespace phoenix {

VideoController::VideoController(QObject *parent)
    : QObject(parent)
{
    spdlog::debug("VideoController created");
}

VideoController::~VideoController() {
    stop();
    spdlog::debug("VideoController destroyed");
}

void VideoController::setSource(const QString& source) {
    if (source_ == source) return;
    
    // Stop current playback
    stop();
    
    source_ = source;
    spdlog::info("Source set to: {}", source.toStdString());
    
    // TODO: Open file with PhoenixEngine
    // auto result = player_->open(source.toStdString());
    // if (!result.ok()) {
    //     emit error(QString::fromStdString(result.error().message()));
    //     return;
    // }
    
    // Simulate loading for now
    durationUs_ = 30 * 1000000;  // 30 seconds mock
    
    emit sourceChanged();
    emit durationChanged();
}

void VideoController::setVolume(float volume) {
    if (qFuzzyCompare(volume_, volume)) return;
    
    volume_ = qBound(0.0f, volume, 1.0f);
    spdlog::debug("Volume set to: {:.1f}", volume_);
    
    // TODO: Set volume on audio sink
    
    emit volumeChanged();
}

void VideoController::play() {
    if (source_.isEmpty()) {
        emit error("No source file loaded");
        return;
    }
    
    spdlog::info("Play");
    
    // TODO: player_->play();
    
    isPlaying_ = true;
    isPaused_ = false;
    
    emit playingChanged();
    emit pausedChanged();
}

void VideoController::pause() {
    if (!isPlaying_) return;
    
    spdlog::info("Pause");
    
    // TODO: player_->togglePause();
    
    isPaused_ = true;
    
    emit pausedChanged();
}

void VideoController::stop() {
    if (!isPlaying_ && !isPaused_) return;
    
    spdlog::info("Stop");
    
    // TODO: player_->stop();
    
    isPlaying_ = false;
    isPaused_ = false;
    positionUs_ = 0;
    
    emit playingChanged();
    emit pausedChanged();
    emit positionChanged();
}

void VideoController::seek(qint64 positionMs) {
    qint64 positionUs = positionMs * 1000;
    
    spdlog::debug("Seek to: {} ms", positionMs);
    
    // TODO: player_->seek(positionUs);
    
    positionUs_ = positionUs;
    emit positionChanged();
}

void VideoController::togglePlayPause() {
    if (isPaused_ || !isPlaying_) {
        play();
    } else {
        pause();
    }
}

void VideoController::updatePosition() {
    // TODO: Get position from player clock
    // positionUs_ = player_->clock().currentTime();
    emit positionChanged();
}

} // namespace phoenix


