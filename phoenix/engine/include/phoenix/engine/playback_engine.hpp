/**
 * @file playback_engine.hpp
 * @brief Playback engine for real-time video playback
 * 
 * Manages playback state, timing, and frame delivery for
 * real-time preview.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/clock.hpp>
#include <phoenix/core/signals.hpp>
#include <phoenix/model/sequence.hpp>
#include <phoenix/engine/compositor.hpp>
#include <phoenix/engine/frame_cache.hpp>

#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace phoenix::engine {

/**
 * @brief Playback state
 */
enum class PlaybackState {
    Stopped,    ///< Not playing, at start or explicit stop
    Playing,    ///< Active playback
    Paused,     ///< Paused, can resume
    Seeking,    ///< Seeking to new position
};

/**
 * @brief Playback direction
 */
enum class PlaybackDirection {
    Forward,
    Backward,
};

/**
 * @brief Frame ready callback
 */
using FrameCallback = std::function<void(
    std::shared_ptr<media::VideoFrame> frame,
    Timestamp pts
)>;

/**
 * @brief Playback engine for real-time video preview
 * 
 * Manages timing, state, and frame delivery for smooth
 * playback. Integrates with the Compositor for frame
 * generation and MasterClock for timing.
 * 
 * Usage:
 * @code
 *   PlaybackEngine engine;
 *   engine.setSequence(&sequence);
 *   engine.setCompositor(&compositor);
 *   engine.onFrame([](auto frame, auto pts) {
 *       renderer.present(frame);
 *   });
 *   
 *   engine.play();
 * @endcode
 */
class PlaybackEngine {
public:
    PlaybackEngine()
        : m_frameCache(std::make_shared<FrameCache>())
        , m_clock(std::make_shared<MasterClock>()) {}
    
    ~PlaybackEngine() {
        stop();
        if (m_playbackThread.joinable()) {
            m_playbackThread.join();
        }
    }
    
    // Non-copyable
    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;
    
    // ========== Configuration ==========
    
    /**
     * @brief Set the sequence to play
     */
    void setSequence(const model::Sequence* sequence) {
        bool wasPlaying = m_state == PlaybackState::Playing;
        if (wasPlaying) pause();
        
        m_sequence = sequence;
        
        if (sequence) {
            m_duration = sequence->duration();
            m_frameRate = sequence->settings().frameRate;
            m_frameDuration = sequence->settings().frameDuration();
            m_outPoint = m_duration;  // Reset out point to end
        }
        
        if (wasPlaying) play();
    }
    
    /**
     * @brief Set the compositor
     */
    void setCompositor(Compositor* compositor) {
        m_compositor = compositor;
    }
    
    /**
     * @brief Set frame ready callback
     */
    void onFrame(FrameCallback callback) {
        m_frameCallback = std::move(callback);
    }
    
    /**
     * @brief Set playback speed multiplier
     * 
     * @param speed Speed multiplier (1.0 = normal, 2.0 = 2x, 0.5 = half)
     */
    void setPlaybackSpeed(double speed) {
        m_playbackSpeed = std::clamp(speed, 0.1, 8.0);
    }
    
    /**
     * @brief Set loop mode
     */
    void setLooping(bool loop) {
        m_looping = loop;
    }
    
    // ========== Transport Controls ==========
    
    /**
     * @brief Start playback
     */
    void play() {
        if (m_state == PlaybackState::Playing) return;
        
        m_state = PlaybackState::Playing;
        m_clock->resume();
        
        startPlaybackThread();
        
        stateChanged.fire(m_state);
    }
    
    /**
     * @brief Pause playback
     */
    void pause() {
        if (m_state != PlaybackState::Playing) return;
        
        m_state = PlaybackState::Paused;
        m_clock->pause();
        
        stateChanged.fire(m_state);
    }
    
    /**
     * @brief Toggle play/pause
     */
    void togglePlayPause() {
        if (m_state == PlaybackState::Playing) {
            pause();
        } else {
            play();
        }
    }
    
    /**
     * @brief Stop playback and return to start
     */
    void stop() {
        m_stopping = true;
        m_state = PlaybackState::Stopped;
        
        if (m_playbackThread.joinable()) {
            m_cv.notify_all();
            m_playbackThread.join();
        }
        
        seek(0);
        m_stopping = false;
        
        stateChanged.fire(m_state);
    }
    
    /**
     * @brief Seek to specific time
     */
    void seek(Timestamp time) {
        auto prevState = m_state.load();
        m_state = PlaybackState::Seeking;
        
        m_currentTime = std::clamp(time, Timestamp(0), m_duration);
        m_clock->seek(m_currentTime);
        
        // Generate frame at new position
        if (m_compositor && m_frameCallback) {
            auto result = m_compositor->compose(m_currentTime);
            if (result.frame) {
                m_frameCallback(result.frame, m_currentTime);
            }
        }
        
        m_state = prevState;
        positionChanged.fire(m_currentTime);
    }
    
    /**
     * @brief Step forward by one frame
     */
    void stepForward() {
        if (m_state == PlaybackState::Playing) pause();
        seek(m_currentTime + m_frameDuration);
    }
    
    /**
     * @brief Step backward by one frame
     */
    void stepBackward() {
        if (m_state == PlaybackState::Playing) pause();
        seek(std::max(Timestamp(0), m_currentTime - m_frameDuration));
    }
    
    /**
     * @brief Go to sequence start
     */
    void goToStart() {
        seek(0);
    }
    
    /**
     * @brief Go to sequence end
     */
    void goToEnd() {
        seek(m_duration);
    }
    
    // ========== In/Out Points ==========
    
    /**
     * @brief Set in point for loop/export
     */
    void setInPoint(Timestamp time) {
        m_inPoint = std::clamp(time, Timestamp(0), m_outPoint);
    }
    
    /**
     * @brief Set out point for loop/export
     */
    void setOutPoint(Timestamp time) {
        m_outPoint = std::clamp(time, m_inPoint, m_duration);
    }
    
    /**
     * @brief Clear in/out points
     */
    void clearInOutPoints() {
        m_inPoint = 0;
        m_outPoint = m_duration;
    }
    
    // ========== State Queries ==========
    
    [[nodiscard]] PlaybackState state() const { return m_state; }
    [[nodiscard]] bool isPlaying() const { return m_state == PlaybackState::Playing; }
    [[nodiscard]] bool isPaused() const { return m_state == PlaybackState::Paused; }
    [[nodiscard]] bool isStopped() const { return m_state == PlaybackState::Stopped; }
    
    [[nodiscard]] Timestamp currentTime() const { return m_currentTime; }
    [[nodiscard]] Timestamp duration() const { return m_duration; }
    [[nodiscard]] Timestamp inPoint() const { return m_inPoint; }
    [[nodiscard]] Timestamp outPoint() const { return m_outPoint; }
    
    [[nodiscard]] double playbackSpeed() const { return m_playbackSpeed; }
    [[nodiscard]] bool isLooping() const { return m_looping; }
    
    [[nodiscard]] const Rational& frameRate() const { return m_frameRate; }
    [[nodiscard]] Duration frameDuration() const { return m_frameDuration; }
    
    [[nodiscard]] std::shared_ptr<FrameCache> frameCache() const { return m_frameCache; }
    [[nodiscard]] std::shared_ptr<MasterClock> clock() const { return m_clock; }
    
    // ========== Signals ==========
    
    Signal<PlaybackState> stateChanged;
    Signal<Timestamp> positionChanged;
    VoidSignal playbackEnded;
    
private:
    void startPlaybackThread() {
        if (m_playbackThread.joinable()) {
            m_cv.notify_all();
            m_playbackThread.join();
        }
        
        m_playbackThread = std::thread([this]() {
            playbackLoop();
        });
    }
    
    void playbackLoop() {
        using namespace std::chrono;
        
        auto lastFrameTime = high_resolution_clock::now();
        
        while (!m_stopping && m_state == PlaybackState::Playing) {
            auto now = high_resolution_clock::now();
            auto elapsed = duration_cast<microseconds>(now - lastFrameTime).count();
            
            // Calculate target frame time with speed adjustment
            Duration targetDuration = static_cast<Duration>(
                m_frameDuration / m_playbackSpeed
            );
            
            if (elapsed >= targetDuration) {
                // Time for next frame
                m_currentTime += m_frameDuration;
                lastFrameTime = now;
                
                // Check for end of sequence
                if (m_currentTime >= m_outPoint) {
                    if (m_looping) {
                        m_currentTime = m_inPoint;
                    } else {
                        m_state = PlaybackState::Stopped;
                        playbackEnded.fire();
                        break;
                    }
                }
                
                // Update clock
                m_clock->update(m_currentTime);
                
                // Compose and deliver frame
                if (m_compositor && m_frameCallback) {
                    auto result = m_compositor->compose(m_currentTime);
                    if (result.frame) {
                        m_frameCallback(result.frame, m_currentTime);
                    }
                }
                
                positionChanged.fire(m_currentTime);
            } else {
                // Sleep until next frame
                auto sleepTime = microseconds(targetDuration - elapsed);
                if (sleepTime.count() > 1000) {
                    std::unique_lock lock(m_mutex);
                    m_cv.wait_for(lock, sleepTime / 2);
                } else {
                    std::this_thread::yield();
                }
            }
        }
    }
    
private:
    // Sequence
    const model::Sequence* m_sequence = nullptr;
    Compositor* m_compositor = nullptr;
    
    // State
    std::atomic<PlaybackState> m_state{PlaybackState::Stopped};
    std::atomic<Timestamp> m_currentTime{0};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_looping{false};
    std::atomic<double> m_playbackSpeed{1.0};
    
    // Timeline
    Duration m_duration = 0;
    Timestamp m_inPoint = 0;
    Timestamp m_outPoint = 0;
    
    // Timing
    Rational m_frameRate{30, 1};
    Duration m_frameDuration = 33333;  // ~30fps in microseconds
    
    // Components
    std::shared_ptr<FrameCache> m_frameCache;
    std::shared_ptr<MasterClock> m_clock;
    
    // Callbacks
    FrameCallback m_frameCallback;
    
    // Threading
    std::thread m_playbackThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

} // namespace phoenix::engine
