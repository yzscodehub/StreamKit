/**
 * @file pipeline.hpp
 * @brief Pipeline manager for Flow Graph orchestration
 * 
 * Manages node lifecycle, connections, and state transitions:
 * - start(): Start downstream first, then source
 * - stop(): Stop InputPins first (wake threads), drain, then stop nodes
 * - seek(): Pause source, flush queues, flushCodec, reset clock, resume
 * - Implements PipelineState machine
 */

#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <chrono>

#include "concepts.hpp"
#include "node.hpp"
#include "async_queue.hpp"
#include "core/types.hpp"
#include "core/clock.hpp"

#include <spdlog/spdlog.h>

namespace phoenix {

// Forward declarations
class Pipeline;

/**
 * @brief Pipeline event callback types
 */
using StateChangeCallback = std::function<void(PipelineState oldState, PipelineState newState)>;
using ErrorCallback = std::function<void(const Error& error)>;
using EofCallback = std::function<void()>;

/**
 * @brief Pipeline configuration
 */
struct PipelineConfig {
    /// Pre-roll timeout (how long to wait for A/V ready)
    std::chrono::milliseconds prerollTimeout{kPrerollTimeoutMs};
    
    /// Whether to auto-start on open
    bool autoStart = false;
    
    /// Whether to loop on EOF
    bool loop = false;
};

/**
 * @brief Pipeline manager - orchestrates the Flow Graph
 * 
 * Lifecycle:
 * 1. Create nodes and add to pipeline
 * 2. Connect nodes using connect() helpers
 * 3. Call start() to begin playback
 * 4. Handle events (state changes, errors, EOF)
 * 5. Call stop() or destroy pipeline
 */
class Pipeline {
public:
    Pipeline() = default;
    
    explicit Pipeline(PipelineConfig config) : config_(std::move(config)) {}
    
    ~Pipeline() {
        stop();
    }
    
    // Non-copyable, non-movable
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;
    
    // ========== Node Management ==========
    
    /**
     * @brief Add a node to the pipeline
     * @param node Node to add (takes ownership)
     */
    void addNode(INodePtr node) {
        nodes_.push_back(std::move(node));
    }
    
    /**
     * @brief Add an async queue node
     */
    template<Transferable T>
    AsyncQueueNode<T>* addAsyncQueue(std::string name, size_t capacity = kDefaultVideoQueueCapacity) {
        auto queue = std::make_unique<AsyncQueueNode<T>>(std::move(name), capacity);
        auto* ptr = queue.get();
        asyncQueues_.push_back(ptr);
        nodes_.push_back(std::move(queue));
        return ptr;
    }
    
    /**
     * @brief Get node by name
     */
    INode* getNode(const std::string& name) {
        for (auto& node : nodes_) {
            if (node->name() == name) {
                return node.get();
            }
        }
        return nullptr;
    }
    
    // ========== Connection Helpers ==========
    
    /**
     * @brief Connect source output to queue input
     */
    template<Transferable T>
    void connect(SourceNode<T>* source, AsyncQueueNode<T>* queue) {
        source->output.connect(&queue->input);
        spdlog::debug("Connected {} -> {}", source->name(), queue->name());
    }
    
    /**
     * @brief Connect queue output to processor input
     */
    template<Transferable T, Transferable TOut>
    void connect(AsyncQueueNode<T>* queue, ProcessorNode<T, TOut>* processor) {
        queue->output.connect(&processor->input);
        spdlog::debug("Connected {} -> {}", queue->name(), processor->name());
    }
    
    /**
     * @brief Connect processor output to queue input
     */
    template<Transferable TIn, Transferable T>
    void connect(ProcessorNode<TIn, T>* processor, AsyncQueueNode<T>* queue) {
        processor->output.connect(&queue->input);
        spdlog::debug("Connected {} -> {}", processor->name(), queue->name());
    }
    
    /**
     * @brief Connect queue output to sink input
     */
    template<Transferable T>
    void connect(AsyncQueueNode<T>* queue, SinkNode<T>* sink) {
        queue->output.connect(&sink->input);
        spdlog::debug("Connected {} -> {}", queue->name(), sink->name());
    }
    
    /**
     * @brief Direct connection: source to processor (same thread)
     */
    template<Transferable T, Transferable TOut>
    void connect(SourceNode<T>* source, ProcessorNode<T, TOut>* processor) {
        source->output.connect(&processor->input);
        spdlog::debug("Connected {} -> {} (direct)", source->name(), processor->name());
    }
    
    /**
     * @brief Direct connection: processor to sink (same thread)
     */
    template<Transferable TIn, Transferable T>
    void connect(ProcessorNode<TIn, T>* processor, SinkNode<T>* sink) {
        processor->output.connect(&sink->input);
        spdlog::debug("Connected {} -> {} (direct)", processor->name(), sink->name());
    }
    
    // ========== Lifecycle ==========
    
    /**
     * @brief Start the pipeline
     * 
     * Starts downstream nodes first, then source.
     * Enters Buffering state and waits for pre-roll.
     */
    void start() {
        if (state_ != PipelineState::Stopped) {
            spdlog::warn("Pipeline::start() called in state {}", 
                pipelineStateToString(state_));
            return;
        }
        
        setState(PipelineState::Buffering);
        bufferingStartTime_ = Clock::now();
        
        // Start nodes in reverse order (downstream first)
        for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
            (*it)->start();
        }
        
        spdlog::info("Pipeline started, waiting for pre-roll...");
    }
    
    /**
     * @brief Stop the pipeline
     * 
     * 1. Stop InputPins first (wake blocked threads)
     * 2. Stop nodes in order (source first)
     */
    void stop() {
        if (state_ == PipelineState::Stopped) {
            return;
        }
        
        spdlog::info("Pipeline stopping...");
        
        // 1. Stop all InputPins first (wake blocked threads)
        // Note: Queue's input.stop() is called in queue->stop()
        
        // 2. Stop nodes in forward order (source first)
        for (auto& node : nodes_) {
            node->stop();
        }
        
        setState(PipelineState::Stopped);
        spdlog::info("Pipeline stopped");
    }
    
    /**
     * @brief Pause playback
     */
    void pause() {
        if (state_ != PipelineState::Playing) {
            return;
        }
        
        clock_.pause();
        setState(PipelineState::Paused);
        spdlog::info("Pipeline paused");
    }
    
    /**
     * @brief Resume playback
     */
    void resume() {
        if (state_ != PipelineState::Paused) {
            return;
        }
        
        clock_.resume();
        setState(PipelineState::Playing);
        spdlog::info("Pipeline resumed");
    }
    
    /**
     * @brief Seek to position
     * 
     * Sequence:
     * 1. Enter Seeking state
     * 2. Flush all queues
     * 3. Flush all processor nodes (decoders)
     * 4. Increment seek serial
     * 5. Reset clock
     * 6. Seek source
     * 7. Return to Playing (or Paused)
     * 
     * @param positionUs Target position in microseconds
     */
    void seek(Timestamp positionUs) {
        PipelineState prevState = state_.load();
        if (prevState == PipelineState::Stopped) {
            return;
        }
        
        setState(PipelineState::Seeking);
        
        // Increment serial FIRST
        seekSerial_.fetch_add(1, std::memory_order_release);
        
        // Flush all queues
        for (auto* queue : asyncQueues_) {
            queue->flush();
        }
        
        // Flush all nodes (decoders will call avcodec_flush_buffers)
        for (auto& node : nodes_) {
            node->flush();
        }
        
        // Reset clock to target position
        clock_.seek(positionUs);
        
        // Notify source to seek (implement in subclass or callback)
        if (seekCallback_) {
            seekCallback_(positionUs);
        }
        
        // Return to previous state (or Playing)
        setState(prevState == PipelineState::Paused ? PipelineState::Paused : PipelineState::Playing);
        
        spdlog::info("Seeked to {} us", positionUs);
    }
    
    // ========== Pre-roll ==========
    
    /**
     * @brief Notify that video is ready (first frame received)
     */
    void notifyVideoReady() {
        videoReady_.store(true, std::memory_order_release);
        checkPrerollComplete();
    }
    
    /**
     * @brief Notify that audio is ready (device opened)
     */
    void notifyAudioReady() {
        audioReady_.store(true, std::memory_order_release);
        checkPrerollComplete();
    }
    
    /**
     * @brief Check and handle pre-roll completion
     */
    void checkPrerollComplete() {
        if (state_ != PipelineState::Buffering) {
            return;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - bufferingStartTime_
        );
        
        bool videoOk = videoReady_.load(std::memory_order_acquire);
        bool audioOk = audioReady_.load(std::memory_order_acquire);
        bool timeout = elapsed >= config_.prerollTimeout;
        
        if (videoOk && audioOk) {
            // Both ready - start!
            startPlayback();
        } else if (videoOk && (timeout || !hasAudioStream_)) {
            // Video ready but audio timed out or no audio stream
            spdlog::warn("Audio pre-roll timeout, using wall clock");
            clock_.useWallClock();
            clock_.setAudioSource(false);
            startPlayback();
        }
        // If only audio ready, keep waiting (video is essential)
    }
    
    /**
     * @brief Handle EOF from source
     */
    void notifyEof() {
        spdlog::info("Pipeline received EOF");
        
        if (config_.loop) {
            seek(0);  // Loop back to start
        } else {
            if (eofCallback_) {
                eofCallback_();
            }
        }
    }
    
    // ========== State Queries ==========
    
    [[nodiscard]] PipelineState state() const {
        return state_.load(std::memory_order_acquire);
    }
    
    [[nodiscard]] bool isPlaying() const {
        return state_ == PipelineState::Playing;
    }
    
    [[nodiscard]] bool isPaused() const {
        return state_ == PipelineState::Paused;
    }
    
    [[nodiscard]] bool isStopped() const {
        return state_ == PipelineState::Stopped;
    }
    
    [[nodiscard]] uint64_t currentSerial() const {
        return seekSerial_.load(std::memory_order_acquire);
    }
    
    // ========== Clock Access ==========
    
    MasterClock& clock() { return clock_; }
    const MasterClock& clock() const { return clock_; }
    
    // ========== Configuration ==========
    
    void setHasAudioStream(bool hasAudio) {
        hasAudioStream_ = hasAudio;
        clock_.setAudioSource(hasAudio);
    }
    
    // ========== Callbacks ==========
    
    void setStateChangeCallback(StateChangeCallback cb) {
        stateChangeCallback_ = std::move(cb);
    }
    
    void setErrorCallback(ErrorCallback cb) {
        errorCallback_ = std::move(cb);
    }
    
    void setEofCallback(EofCallback cb) {
        eofCallback_ = std::move(cb);
    }
    
    void setSeekCallback(std::function<void(Timestamp)> cb) {
        seekCallback_ = std::move(cb);
    }
    
private:
    void setState(PipelineState newState) {
        PipelineState oldState = state_.exchange(newState, std::memory_order_acq_rel);
        if (oldState != newState && stateChangeCallback_) {
            stateChangeCallback_(oldState, newState);
        }
    }
    
    void startPlayback() {
        clock_.start();
        setState(PipelineState::Playing);
        spdlog::info("Pipeline playing");
    }
    
    // Configuration
    PipelineConfig config_;
    
    // Nodes
    std::vector<INodePtr> nodes_;
    std::vector<INode*> asyncQueues_;  // Raw pointers to async queues for flush
    
    // State
    std::atomic<PipelineState> state_{PipelineState::Stopped};
    std::atomic<uint64_t> seekSerial_{0};
    
    // Pre-roll
    std::atomic<bool> videoReady_{false};
    std::atomic<bool> audioReady_{false};
    TimePoint bufferingStartTime_;
    bool hasAudioStream_ = false;
    
    // Clock
    MasterClock clock_;
    
    // Callbacks
    StateChangeCallback stateChangeCallback_;
    ErrorCallback errorCallback_;
    EofCallback eofCallback_;
    std::function<void(Timestamp)> seekCallback_;
};

} // namespace phoenix

