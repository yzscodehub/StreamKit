/**
 * @file video_sink_node.hpp
 * @brief VideoSinkNode - Renders video frames with A/V sync
 * 
 * Key features:
 * - Adaptive A/V sync (drop/wait/present)
 * - Pre-roll support (hold first frame until ready)
 * - Serial number filtering (reject stale frames after seek)
 * - Frame drop/render statistics
 */

#pragma once

#include <thread>
#include <atomic>
#include <functional>

#include "graph/node.hpp"
#include "core/types.hpp"
#include "core/media_frame.hpp"
#include "core/clock.hpp"
#include "render/renderer.hpp"

#include <spdlog/spdlog.h>

namespace phoenix {

/**
 * @brief Callback type for notifying pipeline of events
 */
using ReadyCallback = std::function<void()>;
using EofCallback = std::function<void()>;
using SinkErrorCallback = std::function<void(const std::string&)>;

/**
 * @brief Video sink node with A/V sync
 * 
 * Runs a worker thread that pulls frames from input,
 * syncs with master clock, and renders.
 */
class VideoSinkNode : public NodeBase {
public:
    explicit VideoSinkNode(std::string name = "VideoSink",
                          size_t inputCapacity = kDefaultVideoQueueCapacity)
        : NodeBase(std::move(name))
        , input(inputCapacity)
    {}
    
    ~VideoSinkNode() override {
        stop();
    }
    
    /// Input pin for video frames
    InputPin<VideoFrame> input;
    
    // ========== Configuration ==========
    
    /**
     * @brief Set the renderer
     */
    void setRenderer(IRenderer* renderer) {
        renderer_ = renderer;
    }
    
    /**
     * @brief Set the master clock for A/V sync
     */
    void setClock(MasterClock* clock) {
        clock_ = clock;
    }
    
    /**
     * @brief Set current serial for seek filtering
     */
    void setSerial(uint64_t serial) {
        currentSerial_.store(serial, std::memory_order_release);
    }
    
    /**
     * @brief Set callbacks
     */
    void setReadyCallback(ReadyCallback cb) { readyCallback_ = std::move(cb); }
    void setEofCallback(EofCallback cb) { eofCallback_ = std::move(cb); }
    void setErrorCallback(SinkErrorCallback cb) { errorCallback_ = std::move(cb); }
    
    // ========== Lifecycle ==========
    
    void start() override {
        if (running_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        
        firstFrame_ = true;
        input.reset();
        
        // Start worker thread
        worker_ = std::thread([this] { workerLoop(); });
        
        spdlog::info("[{}] Started", name_);
    }
    
    void stop() override {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        
        input.stop();
        
        if (worker_.joinable()) {
            worker_.join();
        }
        
        spdlog::info("[{}] Stopped. Rendered: {}, Dropped: {}", 
            name_, framesRendered_.load(), framesDropped_.load());
    }
    
    void flush() override {
        input.flush();
        firstFrame_ = true;
        spdlog::debug("[{}] Flushed", name_);
    }
    
    // ========== Statistics ==========
    
    [[nodiscard]] uint64_t framesRendered() const {
        return framesRendered_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] uint64_t framesDropped() const {
        return framesDropped_.load(std::memory_order_relaxed);
    }
    
private:
    /**
     * @brief Worker thread loop
     */
    void workerLoop() {
        spdlog::debug("[{}] Worker started", name_);
        
        while (running_.load(std::memory_order_acquire)) {
            // Pop frame from input with short timeout for faster shutdown
            auto [result, frame] = input.pop(std::chrono::milliseconds(50));
            
            if (result == PopResult::Terminated) {
                break;
            }
            
            if (result == PopResult::Timeout) {
                continue;
            }
            
            if (!frame.has_value()) {
                continue;
            }
            
            // Check running again before processing (for faster shutdown)
            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
            
            // Process the frame
            consume(std::move(*frame));
        }
        
        spdlog::debug("[{}] Worker exited", name_);
    }
    
    /**
     * @brief Consume and render a frame
     */
    void consume(VideoFrame frame) {
        // Check for EOF
        if (frame.isEof()) {
            spdlog::info("[{}] Received EOF", name_);
            if (eofCallback_) {
                eofCallback_();
            }
            return;
        }
        
        // Check for error frame
        if (frame.isError()) {
            consecutiveErrors_++;
            spdlog::error("[{}] Received error frame (consecutive: {})", name_, consecutiveErrors_);
            
            if (consecutiveErrors_ >= kMaxConsecutiveDecoderErrors) {
                spdlog::error("[{}] Too many consecutive errors, notifying pipeline", name_);
                if (errorCallback_) {
                    errorCallback_("Too many consecutive decode errors");
                }
            }
            return;
        }
        
        // Reset error count on valid frame
        consecutiveErrors_ = 0;
        
        // Filter stale frames (from before seek)
        uint64_t expectedSerial = currentSerial_.load(std::memory_order_acquire);
        if (frame.serial != expectedSerial) {
            spdlog::trace("[{}] Dropping stale frame (serial {} != {})", 
                name_, frame.serial, expectedSerial);
            framesDropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        
        // Handle first frame (pre-roll)
        if (firstFrame_) {
            firstFrame_ = false;
            spdlog::debug("[{}] First frame received, pts={}", name_, frame.pts);
            
            if (readyCallback_) {
                readyCallback_();
            }
        }
        
        // A/V sync
        if (clock_) {
            SyncAction action = syncFrame(frame);
            
            switch (action) {
                case SyncAction::Drop:
                    framesDropped_.fetch_add(1, std::memory_order_relaxed);
                    spdlog::trace("[{}] Dropping late frame pts={}", name_, frame.pts);
                    return;
                    
                case SyncAction::Wait:
                    // Sleep until presentation time
                    waitForPresentation(frame.pts);
                    break;
                    
                case SyncAction::Present:
                    // Present immediately
                    break;
            }
        }
        
        // Render the frame
        renderFrame(frame);
    }
    
    /**
     * @brief Determine sync action for frame
     */
    SyncAction syncFrame(const VideoFrame& frame) {
        if (!clock_ || frame.pts == kNoTimestamp) {
            return SyncAction::Present;
        }
        
        Duration delay = clock_->untilPresent(frame.pts);
        
        if (delay > kSyncWaitThreshold) {
            // Video WAY too fast (>500ms early)
            return SyncAction::Wait;
        } else if (delay < kSyncDropThreshold) {
            // Video too late (>100ms behind)
            return SyncAction::Drop;
        } else if (delay < kSyncRushThreshold) {
            // Slightly late - present immediately
            return SyncAction::Present;
        } else if (delay > 0) {
            // Normal case - wait then present
            return SyncAction::Wait;
        } else {
            // Within tolerance - present now
            return SyncAction::Present;
        }
    }
    
    /**
     * @brief Wait until frame presentation time
     */
    void waitForPresentation(Timestamp pts) {
        if (!clock_) return;
        
        Duration delay = clock_->untilPresent(pts);
        
        if (delay > 0 && delay < kSyncWaitThreshold) {
            // Sleep for the delay (minus a small margin)
            auto sleepTime = std::chrono::microseconds(delay - 1000);
            if (sleepTime.count() > 0) {
                std::this_thread::sleep_for(sleepTime);
            }
        }
    }
    
    /**
     * @brief Render a frame to the display
     */
    void renderFrame(const VideoFrame& frame) {
        if (!renderer_) {
            return;
        }
        
        auto result = renderer_->draw(frame);
        if (result.ok()) {
            renderer_->present();
            framesRendered_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::warn("[{}] Render failed: {}", name_, result.error().what());
        }
    }
    
    // Dependencies
    IRenderer* renderer_ = nullptr;
    MasterClock* clock_ = nullptr;
    
    // Serial for seek filtering
    std::atomic<uint64_t> currentSerial_{0};
    
    // Pre-roll state
    bool firstFrame_ = true;
    
    // Callbacks
    ReadyCallback readyCallback_;
    EofCallback eofCallback_;
    SinkErrorCallback errorCallback_;
    
    // Error tracking
    int consecutiveErrors_ = 0;
    
    // Worker thread
    std::thread worker_;
    
    // Statistics
    std::atomic<uint64_t> framesRendered_{0};
    std::atomic<uint64_t> framesDropped_{0};
};

} // namespace phoenix

