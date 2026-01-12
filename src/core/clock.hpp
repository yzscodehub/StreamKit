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
        uint64_t seq = sequence_.load(std::memory_order_relaxed);
        sequence_.store(seq + 1, std::memory_order_release);
        
        // Write data
        baseMediaTime_.store(mediaTime, std::memory_order_relaxed);
        baseRealTime_.store(now_realtime(), std::memory_order_relaxed);
        
        // End write: increment to even
        sequence_.store(seq + 2, std::memory_order_release);
    }
    
    /**
     * @brief Start the clock
     */
    void start() {
        paused_.store(false, std::memory_order_release);
        update(baseMediaTime_.load(std::memory_order_relaxed));
    }
    
    /**
     * @brief Pause the clock
     */
    void pause() {
        if (!paused_.exchange(true, std::memory_order_acq_rel)) {
            // Record the media time at pause
            pausedMediaTime_ = now();
        }
    }
    
    /**
     * @brief Resume from pause
     */
    void resume() {
        if (paused_.exchange(false, std::memory_order_acq_rel)) {
            // Restore from paused media time
            update(pausedMediaTime_);
        }
    }
    
    /**
     * @brief Seek to specific media time
     */
    void seek(Timestamp targetTime) {
        update(targetTime);
        pausedMediaTime_ = targetTime;
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
        if (paused_.load(std::memory_order_acquire)) {
            return pausedMediaTime_;
        }
        
        Timestamp mediaTime;
        int64_t realTime;
        uint64_t seq1, seq2;
        
        // SeqLock read loop
        do {
            seq1 = sequence_.load(std::memory_order_acquire);
            mediaTime = baseMediaTime_.load(std::memory_order_relaxed);
            realTime = baseRealTime_.load(std::memory_order_relaxed);
            seq2 = sequence_.load(std::memory_order_acquire);
        } while (seq1 != seq2 || (seq1 & 1));  // Retry if seq changed or odd
        
        // Interpolate current time
        int64_t elapsed = now_realtime() - realTime;
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
        hasAudioSource_.store(hasAudio, std::memory_order_release);
    }
    
    /**
     * @brief Check if audio is driving the clock
     */
    [[nodiscard]] bool hasAudioSource() const {
        return hasAudioSource_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Switch to wall clock mode (no audio)
     * 
     * In this mode, the clock runs based on system time alone.
     * Call this when there's no audio stream or audio fails.
     */
    void useWallClock() {
        if (!useWallClock_.exchange(true, std::memory_order_acq_rel)) {
            // Initialize wall clock base
            update(baseMediaTime_.load(std::memory_order_relaxed));
        }
    }
    
    /**
     * @brief Check if using wall clock mode
     */
    [[nodiscard]] bool isWallClockMode() const {
        return useWallClock_.load(std::memory_order_acquire);
    }
    
    // ========== State Queries ==========
    
    /**
     * @brief Check if clock is paused
     */
    [[nodiscard]] bool isPaused() const {
        return paused_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Get base media time (without interpolation)
     */
    [[nodiscard]] Timestamp baseTime() const {
        return baseMediaTime_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Reset clock to initial state
     */
    void reset() {
        sequence_.store(0, std::memory_order_release);
        baseMediaTime_.store(0, std::memory_order_release);
        baseRealTime_.store(now_realtime(), std::memory_order_release);
        paused_.store(false, std::memory_order_release);
        pausedMediaTime_ = 0;
        hasAudioSource_.store(false, std::memory_order_release);
        useWallClock_.store(false, std::memory_order_release);
    }
    
private:
    /// Get current real time in microseconds
    [[nodiscard]] static int64_t now_realtime() {
        auto now = Clock::now();
        return std::chrono::duration_cast<Microseconds>(
            now.time_since_epoch()
        ).count();
    }
    
    // SeqLock data
    std::atomic<uint64_t> sequence_{0};
    std::atomic<Timestamp> baseMediaTime_{0};
    std::atomic<int64_t> baseRealTime_{0};
    
    // Pause state
    std::atomic<bool> paused_{false};
    Timestamp pausedMediaTime_ = 0;
    
    // Audio source state
    std::atomic<bool> hasAudioSource_{false};
    std::atomic<bool> useWallClock_{false};
};

} // namespace phoenix

