/**
 * @file clock.hpp
 * @brief Master clock for A/V synchronization using SeqLock pattern
 * 
 * The SeqLock pattern provides:
 * - Lock-free reads in the hot path (video/audio playback)
 * - Single writer (audio callback or wall clock)
 * - Multiple readers (video sink, UI)
 * 
 * This is critical for smooth playback - blocking on a mutex in the
 * render path would cause frame drops.
 */

#pragma once

#include <atomic>
#include <chrono>

#include "types.hpp"

namespace phoenix {

/**
 * @brief Master clock for A/V synchronization
 * 
 * Uses SeqLock pattern:
 * - Sequence number is odd during write, even otherwise
 * - Readers spin until they read the same even sequence before and after
 * 
 * Thread safety:
 * - update(): Single writer only (audio thread or timer)
 * - now(): Multiple readers, lock-free
 * - seek(): Single caller (UI thread)
 */
class MasterClock {
public:
    MasterClock() = default;
    
    // Non-copyable
    MasterClock(const MasterClock&) = delete;
    MasterClock& operator=(const MasterClock&) = delete;
    
    // ========== Writer Interface (Single Thread) ==========
    
    /**
     * @brief Update current media time
     * 
     * Should be called from audio callback to drive the clock,
     * or periodically from a timer in wall-clock mode.
     * 
     * @param mediaTime Current media position in microseconds
     */
    void update(Timestamp mediaTime) {
        // Begin write: increment to odd
        uint64_t seq = m_sequence.load(std::memory_order_relaxed);
        m_sequence.store(seq + 1, std::memory_order_release);
        
        // Write data
        m_baseMediaTime.store(mediaTime, std::memory_order_relaxed);
        m_baseRealTime.store(nowRealtime(), std::memory_order_relaxed);
        
        // End write: increment to even
        m_sequence.store(seq + 2, std::memory_order_release);
    }
    
    /**
     * @brief Start the clock
     */
    void start() {
        m_paused.store(false, std::memory_order_release);
        update(m_baseMediaTime.load(std::memory_order_relaxed));
    }
    
    /**
     * @brief Pause the clock
     */
    void pause() {
        if (!m_paused.exchange(true, std::memory_order_acq_rel)) {
            // Record the media time at pause
            m_pausedMediaTime = now();
        }
    }
    
    /**
     * @brief Resume from pause
     */
    void resume() {
        if (m_paused.exchange(false, std::memory_order_acq_rel)) {
            // Restore from paused media time
            update(m_pausedMediaTime);
        }
    }
    
    /**
     * @brief Seek to specific media time
     */
    void seek(Timestamp targetTime) {
        update(targetTime);
        m_pausedMediaTime = targetTime;
    }
    
    // ========== Reader Interface (Lock-free, Multi-threaded) ==========
    
    /**
     * @brief Get current media time
     * 
     * Lock-free read using SeqLock pattern.
     * May spin briefly if a write is in progress.
     * 
     * @return Current media time in microseconds
     */
    [[nodiscard]] Timestamp now() const {
        if (m_paused.load(std::memory_order_acquire)) {
            return m_pausedMediaTime;
        }
        
        Timestamp mediaTime;
        int64_t realTime;
        uint64_t seq1, seq2;
        
        // SeqLock read loop
        do {
            seq1 = m_sequence.load(std::memory_order_acquire);
            mediaTime = m_baseMediaTime.load(std::memory_order_relaxed);
            realTime = m_baseRealTime.load(std::memory_order_relaxed);
            seq2 = m_sequence.load(std::memory_order_acquire);
        } while (seq1 != seq2 || (seq1 & 1));  // Retry if seq changed or odd
        
        // Interpolate current time
        int64_t elapsed = nowRealtime() - realTime;
        return mediaTime + elapsed;
    }
    
    /**
     * @brief Determine sync action for a frame with given PTS
     * 
     * @param pts Frame presentation timestamp
     * @return SyncAction: Present, Wait, or Drop
     */
    [[nodiscard]] SyncAction shouldPresent(Timestamp pts) const {
        if (pts == kNoTimestamp) {
            return SyncAction::Present;  // No timestamp, present immediately
        }
        
        Timestamp currentTime = now();
        Duration delay = pts - currentTime;
        
        if (delay > kSyncWaitThreshold) {
            return SyncAction::Wait;   // Video too fast
        } else if (delay < kSyncDropThreshold) {
            return SyncAction::Drop;   // Video too late
        } else {
            return SyncAction::Present;
        }
    }
    
    /**
     * @brief Calculate time until frame should be presented
     * 
     * @param pts Frame presentation timestamp
     * @return Microseconds until presentation (negative if late)
     */
    [[nodiscard]] Duration untilPresent(Timestamp pts) const {
        if (pts == kNoTimestamp) {
            return 0;
        }
        return pts - now();
    }
    
    // ========== Audio Source Configuration ==========
    
    /**
     * @brief Set whether audio is driving the clock
     */
    void setAudioSource(bool hasAudio) {
        m_hasAudioSource.store(hasAudio, std::memory_order_release);
    }
    
    /**
     * @brief Check if audio is driving the clock
     */
    [[nodiscard]] bool hasAudioSource() const {
        return m_hasAudioSource.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Switch to wall clock mode (no audio)
     * 
     * In this mode, the clock runs based on system time alone.
     * Call this when there's no audio stream or audio fails.
     */
    void useWallClock() {
        if (!m_useWallClock.exchange(true, std::memory_order_acq_rel)) {
            // Initialize wall clock base
            update(m_baseMediaTime.load(std::memory_order_relaxed));
        }
    }
    
    /**
     * @brief Check if using wall clock mode
     */
    [[nodiscard]] bool isWallClockMode() const {
        return m_useWallClock.load(std::memory_order_acquire);
    }
    
    // ========== State Queries ==========
    
    /**
     * @brief Check if clock is paused
     */
    [[nodiscard]] bool isPaused() const {
        return m_paused.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Get base media time (without interpolation)
     */
    [[nodiscard]] Timestamp baseTime() const {
        return m_baseMediaTime.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Reset clock to initial state
     */
    void reset() {
        m_sequence.store(0, std::memory_order_release);
        m_baseMediaTime.store(0, std::memory_order_release);
        m_baseRealTime.store(nowRealtime(), std::memory_order_release);
        m_paused.store(false, std::memory_order_release);
        m_pausedMediaTime = 0;
        m_hasAudioSource.store(false, std::memory_order_release);
        m_useWallClock.store(false, std::memory_order_release);
    }
    
private:
    /// Get current real time in microseconds
    [[nodiscard]] static int64_t nowRealtime() {
        auto now = Clock::now();
        return std::chrono::duration_cast<Microseconds>(
            now.time_since_epoch()
        ).count();
    }
    
    // SeqLock data
    std::atomic<uint64_t> m_sequence{0};
    std::atomic<Timestamp> m_baseMediaTime{0};
    std::atomic<int64_t> m_baseRealTime{0};
    
    // Pause state
    std::atomic<bool> m_paused{false};
    Timestamp m_pausedMediaTime = 0;
    
    // Audio source state
    std::atomic<bool> m_hasAudioSource{false};
    std::atomic<bool> m_useWallClock{false};
};

} // namespace phoenix
