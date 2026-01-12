/**
 * @file videocontroller.hpp
 * @brief C++ backend for video playback, exposed to QML
 */

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace phoenix {

/**
 * @brief Video playback controller exposed to QML
 * 
 * This bridges the PhoenixEngine core with the QML UI.
 * Provides properties and methods for video control.
 */
class VideoController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    
    // Properties exposed to QML
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY pausedChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit VideoController(QObject *parent = nullptr);
    ~VideoController() override;

    // Property getters
    QString source() const { return source_; }
    qint64 duration() const { return durationUs_ / 1000; }  // Return ms for QML
    qint64 position() const { return positionUs_ / 1000; }
    bool isPlaying() const { return isPlaying_; }
    bool isPaused() const { return isPaused_; }
    float volume() const { return volume_; }

    // Property setters
    void setSource(const QString& source);
    void setVolume(float volume);

public slots:
    // Control methods callable from QML
    void play();
    void pause();
    void stop();
    void seek(qint64 positionMs);
    void togglePlayPause();

signals:
    // Signals for QML bindings
    void sourceChanged();
    void durationChanged();
    void positionChanged();
    void playingChanged();
    void pausedChanged();
    void volumeChanged();
    void error(const QString& message);

private:
    void updatePosition();

    QString source_;
    qint64 durationUs_ = 0;
    qint64 positionUs_ = 0;
    bool isPlaying_ = false;
    bool isPaused_ = false;
    float volume_ = 1.0f;
    
    // TODO: Add PhoenixEngine player instance here
    // std::unique_ptr<Player> player_;
};

} // namespace phoenix


