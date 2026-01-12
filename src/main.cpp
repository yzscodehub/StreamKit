/**
 * @file main.cpp
 * @brief PhoenixEngine - Modern C++20 Video Player
 * 
 * MVP implementation using Flow Graph architecture:
 * FileSource -> [Queue] -> Decoder -> [Queue] -> VideoSink -> Renderer
 */

#include <iostream>
#include <cstdlib>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#endif

// Core
#include "core/types.hpp"
#include "core/result.hpp"
#include "core/media_frame.hpp"
#include "core/clock.hpp"

// Graph
#include "graph/concepts.hpp"
#include "graph/pin.hpp"
#include "graph/node.hpp"
#include "graph/async_queue.hpp"
#include "graph/pipeline.hpp"

// Nodes
#include "nodes/source_node.hpp"
#include "nodes/decode_node.hpp"
#include "nodes/video_sink_node.hpp"
#include "nodes/audio_decode_node.hpp"
#include "nodes/audio_sink_node.hpp"

// Render
#include "render/renderer.hpp"
#include "render/sdl_renderer.hpp"

// FFmpeg version check
extern "C" {
#include <libavcodec/avcodec.h>
}

#if LIBAVCODEC_VERSION_MAJOR < 59
    #error "FFmpeg 5.0+ required (libavcodec 59+). Please update FFmpeg."
#endif

// SDL2
#include <SDL2/SDL.h>

// Logging
#include <spdlog/spdlog.h>

namespace phoenix {

// Global quit flag for signal handling
static std::atomic<bool> g_quitRequested{false};

#ifdef _WIN32
/**
 * @brief Windows Console Control Handler
 * 
 * This is called on a separate thread by Windows when Ctrl+C is pressed.
 * More reliable than signal() on Windows.
 */
BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
            // Note: Can't use spdlog here - may not be safe from this thread
            std::cerr << "\n[WARN] Ctrl+C received, initiating shutdown...\n";
            g_quitRequested.store(true);
            return TRUE;  // We handled it
            
        case CTRL_BREAK_EVENT:
            std::cerr << "\n[WARN] Ctrl+Break received, initiating shutdown...\n";
            g_quitRequested.store(true);
            return TRUE;
            
        case CTRL_CLOSE_EVENT:
            std::cerr << "\n[WARN] Console close received, initiating shutdown...\n";
            g_quitRequested.store(true);
            // Give time for cleanup
            Sleep(2000);
            return TRUE;
            
        default:
            return FALSE;
    }
}
#endif

/**
 * @brief Signal handler for graceful shutdown (Unix/fallback)
 */
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cerr << "\n[WARN] Signal " << signal << " received, initiating shutdown...\n";
        g_quitRequested.store(true);
    }
}

/**
 * @brief Install signal handlers
 */
void installSignalHandlers() {
#ifdef _WIN32
    // Windows: Use Console Control Handler (more reliable than signal())
    if (!SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)) {
        spdlog::warn("Failed to set console control handler");
    }
#endif
    
    // Also set standard signal handlers as fallback
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif
    
    spdlog::debug("Signal handlers installed");
}

/**
 * @brief Event callback type
 */
using EventCallback = std::function<void(PlayerEvent, const std::string&)>;

/**
 * @brief Player application class
 * 
 * Manages the video playback pipeline and event loop.
 * Implements complete pre-roll state machine with timeout.
 */
class Player {
public:
    Player() = default;
    ~Player() { shutdown(); }
    
    /**
     * @brief Set event callback for UI notifications
     */
    void setEventCallback(EventCallback cb) {
        eventCallback_ = std::move(cb);
    }
    
    /**
     * @brief Open a media file
     */
    Result<void> open(const std::string& filename) {
        // Create nodes
        source_ = std::make_unique<FileSourceNode>("FileSource");
        videoDecoder_ = std::make_unique<FFmpegDecodeNode>("VideoDecoder");
        videoSink_ = std::make_unique<VideoSinkNode>("VideoSink");
        
        // Create async queues for video
        videoPacketQueue_ = std::make_unique<AsyncQueueNode<Packet>>("VideoPacketQueue", 50);
        videoFrameQueue_ = std::make_unique<AsyncQueueNode<VideoFrame>>("VideoFrameQueue", 30);
        
        // Create renderer
        renderer_ = createRenderer();
        
        // Open file
        auto result = source_->open(filename);
        if (!result.ok()) {
            return result;
        }
        
        // Check for video stream
        if (!source_->hasVideoStream()) {
            return Err(ErrorCode::NotFound, "No video stream found");
        }
        
        // Initialize video decoder
        result = videoDecoder_->init(source_->getVideoStream());
        if (!result.ok()) {
            return result;
        }
        
        // Get video dimensions for window
        auto* codecCtx = videoDecoder_->codecContext();
        int width = codecCtx->width;
        int height = codecCtx->height;
        
        // Initialize renderer
        result = renderer_->init(width, height, "PhoenixEngine");
        if (!result.ok()) {
            return result;
        }
        
        // Configure video sink
        videoSink_->setRenderer(renderer_.get());
        videoSink_->setClock(&clock_);
        videoSink_->setReadyCallback([this] {
            onVideoReady();
        });
        videoSink_->setEofCallback([this] {
            onVideoEof();
        });
        videoSink_->setErrorCallback([this](const std::string& error) {
            onError("Video: " + error);
        });
        
        // Build video pipeline:
        // Source -> VideoPacketQueue -> VideoDecoder -> VideoFrameQueue -> VideoSink
        source_->videoOutput.connect(&videoPacketQueue_->input);
        videoPacketQueue_->output.connect(&videoDecoder_->input);
        videoDecoder_->output.connect(&videoFrameQueue_->input);
        videoFrameQueue_->output.connect(&videoSink_->input);
        
        // Initialize audio if available
        hasAudio_ = false;
        if (source_->hasAudioStream()) {
            result = initAudio();
            if (result.ok()) {
                hasAudio_ = true;
                spdlog::info("Audio stream initialized");
            } else {
                spdlog::warn("Audio init failed, continuing without audio: {}", 
                    result.error().what());
            }
        } else {
            spdlog::info("No audio stream in file");
        }
        
        isOpen_ = true;
        spdlog::info("Opened: {} ({}x{}){}", filename, width, height,
            hasAudio_ ? " with audio" : " (video only)");
        
        return Ok();
    }
    
    /**
     * @brief Initialize audio pipeline
     */
    Result<void> initAudio() {
        // Create audio nodes
        audioDecoder_ = std::make_unique<AudioDecodeNode>("AudioDecoder");
        audioSink_ = std::make_unique<AudioSinkNode>("AudioSink");
        
        // Create async queues for audio (larger queue for audio)
        audioPacketQueue_ = std::make_unique<AsyncQueueNode<Packet>>("AudioPacketQueue", 1000);
        audioFrameQueue_ = std::make_unique<AsyncQueueNode<AudioFrame>>("AudioFrameQueue", 100);
        
        // Initialize audio decoder
        auto result = audioDecoder_->init(source_->getAudioStream());
        if (!result.ok()) {
            return result;
        }
        
        // Initialize audio sink with resampler
        result = audioSink_->init(audioDecoder_->codecContext());
        if (!result.ok()) {
            return result;
        }
        
        // Configure audio sink
        audioSink_->setClock(&clock_);
        audioSink_->setReadyCallback([this] {
            onAudioReady();
        });
        audioSink_->setEofCallback([this] {
            onAudioEof();
        });
        audioSink_->setErrorCallback([this](const std::string& error) {
            onError("Audio: " + error);
        });
        
        // Build audio pipeline:
        // Source -> AudioPacketQueue -> AudioDecoder -> AudioFrameQueue -> AudioSink
        source_->audioOutput.connect(&audioPacketQueue_->input);
        audioPacketQueue_->output.connect(&audioDecoder_->input);
        audioDecoder_->output.connect(&audioFrameQueue_->input);
        audioFrameQueue_->output.connect(&audioSink_->input);
        
        return Ok();
    }
    
    /**
     * @brief Start playback with pre-roll
     */
    void play() {
        if (!isOpen_) {
            spdlog::error("No file open");
            emitEvent(PlayerEvent::Error, "No file open");
            return;
        }
        
        if (isPlaying_) {
            return;
        }
        
        // Enter buffering state
        pipelineState_ = PipelineState::Buffering;
        bufferingState_ = hasAudio_ ? BufferingState::WaitingBoth : BufferingState::WaitingVideo;
        bufferingStartTime_ = Clock::now();
        emitEvent(PlayerEvent::Buffering, "Starting pre-roll...");
        
        // Configure clock based on audio availability
        if (hasAudio_) {
            clock_.setAudioSource(true);
            spdlog::debug("Clock will be driven by audio");
        } else {
            clock_.useWallClock();
            clock_.setAudioSource(false);
            spdlog::debug("Clock in wall-clock mode (no audio)");
        }
        
        // Start video nodes in reverse order (downstream first)
        videoSink_->start();
        videoFrameQueue_->start();
        videoDecoder_->start();
        videoPacketQueue_->start();
        
        // Start audio nodes if available
        if (hasAudio_) {
            audioSink_->start();
            audioFrameQueue_->start();
            audioDecoder_->start();
            audioPacketQueue_->start();
        }
        
        // Start source last
        source_->start();
        
        // Start decode threads
        videoDecodeThread_ = std::thread([this] {
            videoDecodeLoop();
        });
        
        if (hasAudio_) {
            audioDecodeThread_ = std::thread([this] {
                audioDecodeLoop();
            });
        }
        
        isPlaying_ = true;
        spdlog::info("Playback started{}, waiting for pre-roll...", hasAudio_ ? " with audio" : "");
    }
    
    /**
     * @brief Stop playback
     */
    void stop() {
        if (!isPlaying_) {
            return;
        }
        
        spdlog::info("Stopping playback...");
        isPlaying_ = false;
        
        // ========== Step 1: Stop ALL Input Pins FIRST ==========
        // This is CRITICAL to prevent deadlocks. We must wake up all 
        // blocked producers (like Source) before trying to join them.
        spdlog::debug("Step 1: Stopping input pins");
        
        // Stop Packet Queue Inputs (Unblocks Source)
        videoPacketQueue_->input.stop();
        if (hasAudio_ && audioPacketQueue_) {
            audioPacketQueue_->input.stop();
        }
        
        // Stop Frame Queue Inputs (Unblocks Decoder)
        videoFrameQueue_->input.stop();
        if (hasAudio_ && audioFrameQueue_) {
            audioFrameQueue_->input.stop();
        }
        
        // Stop Decoder Inputs (Unblocks Packet Queue)
        videoDecoder_->input.stop();
        if (hasAudio_ && audioDecoder_) {
            audioDecoder_->input.stop();
        }
        
        // Stop Sink Inputs (Unblocks Frame Queue)
        videoSink_->input.stop();
        if (hasAudio_ && audioSink_) {
            audioSink_->input.stop();
        }
        
        // ========== Step 2: Stop Source ==========
        // Now safe to join source worker because its outputs are stopped
        spdlog::debug("Step 2: Stopping source");
        source_->stop();
        
        // ========== Step 3: Flush ALL Queues ==========
        spdlog::debug("Step 3: Flushing queues");
        
        videoPacketQueue_->flush();
        if (hasAudio_ && audioPacketQueue_) {
            audioPacketQueue_->flush();
        }
        
        videoFrameQueue_->flush();
        if (hasAudio_ && audioFrameQueue_) {
            audioFrameQueue_->flush();
        }
        
        videoSink_->input.flush();
        if (hasAudio_ && audioSink_) {
            audioSink_->input.flush();
        }
        
        // ========== Step 4: Wait for Decode Threads ==========
        spdlog::debug("Step 4: Joining decode threads");
        if (videoDecodeThread_.joinable()) {
            spdlog::debug("Waiting for video decode thread...");
            videoDecodeThread_.join();
            spdlog::debug("Video decode thread joined");
        }
        
        if (hasAudio_ && audioDecodeThread_.joinable()) {
            spdlog::debug("Waiting for audio decode thread...");
            audioDecodeThread_.join();
            spdlog::debug("Audio decode thread joined");
        }
        
        // ========== Step 5: Stop All Nodes (Join Workers) ==========
        spdlog::debug("Step 5: Stopping nodes and joining workers");
        
        // Stop Queues
        spdlog::debug("Stopping videoPacketQueue");
        videoPacketQueue_->stop();
        if (hasAudio_ && audioPacketQueue_) {
            spdlog::debug("Stopping audioPacketQueue");
            audioPacketQueue_->stop();
        }
        
        spdlog::debug("Stopping videoFrameQueue");
        videoFrameQueue_->stop();
        if (hasAudio_ && audioFrameQueue_) {
            spdlog::debug("Stopping audioFrameQueue");
            audioFrameQueue_->stop();
        }
        
        // Stop Decoders
        spdlog::debug("Stopping videoDecoder");
        videoDecoder_->stop();
        if (hasAudio_ && audioDecoder_) {
            spdlog::debug("Stopping audioDecoder");
            audioDecoder_->stop();
        }
        
        // Stop Sinks
        spdlog::debug("Stopping videoSink");
        videoSink_->stop();
        if (hasAudio_ && audioSink_) {
            spdlog::debug("Stopping audioSink");
            audioSink_->stop();
        }
        
        spdlog::info("Playback stopped successfully");
    }
    
    /**
     * @brief Request quit (for external triggers)
     */
    void requestQuit() {
        quitRequested_ = true;
        spdlog::info("Quit requested");
    }
    
    /**
     * @brief Check if quit was requested
     */
    bool isQuitRequested() const {
        return quitRequested_ || g_quitRequested.load();
    }
    
    /**
     * @brief Shutdown and cleanup
     */
    void shutdown() {
        if (!isOpen_) {
            return;
        }
        
        spdlog::info("Shutting down player...");
        emitEvent(PlayerEvent::Stopped, "Shutting down...");
        
        // Stop playback first
        stop();
        
        // Shutdown renderer
        if (renderer_) {
            spdlog::debug("Shutting down renderer");
            renderer_->shutdown();
        }
        
        // Reset video nodes in reverse order (downstream first)
        spdlog::debug("Cleaning up video nodes");
        videoSink_.reset();
        videoFrameQueue_.reset();
        videoDecoder_.reset();
        videoPacketQueue_.reset();
        
        // Reset audio nodes
        if (hasAudio_) {
            spdlog::debug("Cleaning up audio nodes");
            audioSink_.reset();
            audioFrameQueue_.reset();
            audioDecoder_.reset();
            audioPacketQueue_.reset();
        }
        
        source_.reset();
        renderer_.reset();
        
        hasAudio_ = false;
        isOpen_ = false;
        pipelineState_ = PipelineState::Stopped;
        bufferingState_ = BufferingState::Idle;
        spdlog::info("Player shutdown complete");
    }
    
    /**
     * @brief Check if still playing
     */
    bool isPlaying() const { return isPlaying_ && !eofReached_; }
    
    /**
     * @brief Check pre-roll timeout (call periodically from event loop)
     */
    void checkPrerollTimeout() {
        if (bufferingState_ == BufferingState::Ready || 
            bufferingState_ == BufferingState::Timeout ||
            bufferingState_ == BufferingState::Idle) {
            return;
        }
        
        auto elapsed = std::chrono::duration_cast<Milliseconds>(
            Clock::now() - bufferingStartTime_).count();
        
        if (elapsed > kPrerollTimeoutMs) {
            completePreroll(true);
        }
    }
    
    /**
     * @brief Get current pipeline state
     */
    PipelineState pipelineState() const { return pipelineState_; }
    
    /**
     * @brief Get current buffering state
     */
    BufferingState bufferingState() const { return bufferingState_; }
    
    /**
     * @brief Process SDL events
     * @return true to continue, false to quit
     */
    bool processEvents() {
        // Check global quit flag first
        if (g_quitRequested.load()) {
            spdlog::info("Global quit requested");
            return false;
        }
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    spdlog::info("SDL quit event received");
                    return false;
                    
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_q:
                            spdlog::info("Quit key pressed");
                            return false;
                            
                        case SDLK_SPACE:
                            togglePause();
                            break;
                            
                        case SDLK_LEFT:
                            seekRelative(-5'000'000);  // -5 seconds
                            break;
                            
                        case SDLK_RIGHT:
                            seekRelative(5'000'000);   // +5 seconds
                            break;
                            
                        default:
                            break;
                    }
                    break;
                    
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        spdlog::info("Window close event received");
                        return false;
                    }
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        renderer_->resize(event.window.data1, event.window.data2);
                    }
                    break;
            }
        }
        
        return !quitRequested_;
    }
    
    /**
     * @brief Toggle pause state
     */
    void togglePause() {
        if (!isPlaying_) return;
        
        if (isPaused_) {
            // Resume
            source_->resume();
            clock_.resume();
            
            // Resume audio sink (will unpause SDL audio)
            if (hasAudio_ && audioSink_) {
                audioSink_->setPaused(false);
            }
            
            isPaused_ = false;
            pipelineState_ = PipelineState::Playing;
            emitEvent(PlayerEvent::Playing, "Resumed");
            spdlog::info("Resumed");
        } else {
            // Pause
            source_->pause();
            clock_.pause();
            
            // Pause audio sink (will output silence)
            if (hasAudio_ && audioSink_) {
                audioSink_->setPaused(true);
            }
            
            isPaused_ = true;
            pipelineState_ = PipelineState::Paused;
            emitEvent(PlayerEvent::Paused, "");
            spdlog::info("Paused");
        }
    }
    
    /**
     * @brief Seek relative to current position
     */
    void seekRelative(Duration offsetUs) {
        if (!isPlaying_) return;
        
        Timestamp current = clock_.now();
        Timestamp target = current + offsetUs;
        if (target < 0) target = 0;
        
        seek(target);
    }
    
    /**
     * @brief Seek to absolute position
     */
    void seek(Timestamp targetUs) {
        if (!isPlaying_) return;
        
        spdlog::info("Seeking to {} ms", targetUs / 1000);
        
        PipelineState prevState = pipelineState_;
        pipelineState_ = PipelineState::Seeking;
        emitEvent(PlayerEvent::Seeking, std::format("Seeking to {}ms", targetUs / 1000));
        
        // Increment serial to invalidate in-flight frames
        uint64_t newSerial = ++seekSerial_;
        source_->setSerial(newSerial);
        videoSink_->setSerial(newSerial);
        
        if (hasAudio_ && audioSink_) {
            audioSink_->setSerial(newSerial);
        }
        
        // Pause source during seek
        source_->pause();
        
        // Flush video queues and decoder
        videoPacketQueue_->flush();
        videoFrameQueue_->flush();
        videoDecoder_->flush();
        
        // Flush audio queues and decoder (includes ring buffer)
        if (hasAudio_) {
            if (audioPacketQueue_) audioPacketQueue_->flush();
            if (audioFrameQueue_) audioFrameQueue_->flush();
            if (audioDecoder_) audioDecoder_->flush();
            if (audioSink_) audioSink_->flush();
        }
        
        // Reset ready flags for re-buffering
        videoReady_ = false;
        audioReady_ = false;
        bufferingState_ = hasAudio_ ? BufferingState::WaitingBoth : BufferingState::WaitingVideo;
        bufferingStartTime_ = Clock::now();
        
        // Seek the source
        auto result = source_->seekTo(targetUs);
        if (!result.ok()) {
            spdlog::error("Seek failed: {}", result.error().what());
            emitEvent(PlayerEvent::Error, std::format("Seek failed: {}", result.error().what()));
            pipelineState_ = prevState;
            return;
        }
        
        // Update clock
        clock_.seek(targetUs);
        
        // Resume source
        source_->resume();
        
        // Restore state (or wait for buffering to complete)
        pipelineState_ = isPaused_ ? PipelineState::Paused : PipelineState::Buffering;
        emitEvent(PlayerEvent::SeekComplete, "");
    }
    
private:
    /**
     * @brief Emit event to callback
     */
    void emitEvent(PlayerEvent event, const std::string& message = "") {
        spdlog::debug("Event: {} - {}", playerEventToString(event), message);
        if (eventCallback_) {
            eventCallback_(event, message);
        }
    }
    
    /**
     * @brief Called when video sink receives first frame
     */
    void onVideoReady() {
        spdlog::info("Video stream ready (first frame received)");
        videoReady_ = true;
        updateBufferingState();
    }
    
    /**
     * @brief Called when audio sink receives first frame
     */
    void onAudioReady() {
        spdlog::info("Audio stream ready (first frame received)");
        audioReady_ = true;
        updateBufferingState();
    }
    
    /**
     * @brief Update buffering state machine
     */
    void updateBufferingState() {
        if (bufferingState_ == BufferingState::Ready || 
            bufferingState_ == BufferingState::Timeout) {
            return;  // Already completed
        }
        
        // Check what we're waiting for
        bool videoOk = !source_->hasVideoStream() || videoReady_;
        bool audioOk = !hasAudio_ || audioReady_;
        
        if (videoOk && audioOk) {
            // All streams ready!
            completePreroll(false);
        } else if (videoOk && !audioOk) {
            bufferingState_ = BufferingState::WaitingAudio;
            spdlog::debug("Pre-roll: waiting for audio...");
        } else if (!videoOk && audioOk) {
            bufferingState_ = BufferingState::WaitingVideo;
            spdlog::debug("Pre-roll: waiting for video...");
        } else {
            bufferingState_ = BufferingState::WaitingBoth;
        }
    }
    
    /**
     * @brief Complete pre-roll and start actual playback
     */
    void completePreroll(bool timedOut) {
        if (bufferingState_ == BufferingState::Ready) {
            return;  // Already completed
        }
        
        bufferingState_ = timedOut ? BufferingState::Timeout : BufferingState::Ready;
        
        auto elapsed = std::chrono::duration_cast<Milliseconds>(
            Clock::now() - bufferingStartTime_).count();
        
        if (timedOut) {
            spdlog::warn("Pre-roll timeout after {}ms, falling back to wall clock", elapsed);
            clock_.useWallClock();
            emitEvent(PlayerEvent::Warning, "Pre-roll timeout, using fallback sync");
        } else {
            spdlog::info("Pre-roll complete in {}ms, all streams ready", elapsed);
        }
        
        // Start the clock
        if (!clockStarted_) {
            clock_.start();
            clockStarted_ = true;
        }
        
        pipelineState_ = PipelineState::Playing;
        emitEvent(PlayerEvent::BufferingComplete, "");
        emitEvent(PlayerEvent::Playing, "");
    }
    
    /**
     * @brief Called when video sink receives EOF
     */
    void onVideoEof() {
        spdlog::debug("Video EOF");
        videoEofReached_ = true;
        checkAllEof();
    }
    
    /**
     * @brief Called when audio sink receives EOF
     */
    void onAudioEof() {
        spdlog::debug("Audio EOF");
        audioEofReached_ = true;
        checkAllEof();
    }
    
    /**
     * @brief Check if all streams reached EOF
     */
    void checkAllEof() {
        bool allEof = videoEofReached_;
        if (hasAudio_) {
            allEof = allEof && audioEofReached_;
        }
        
        if (allEof && !eofReached_) {
            spdlog::info("End of file reached");
            eofReached_ = true;
            pipelineState_ = PipelineState::Stopped;
            emitEvent(PlayerEvent::EndOfFile, "Playback complete");
        }
    }
    
    /**
     * @brief Handle error from sinks
     */
    void onError(const std::string& error) {
        consecutiveErrors_++;
        spdlog::error("Pipeline error: {} (count: {})", error, consecutiveErrors_);
        
        // For now, just emit the event - the pipeline continues
        // In a more robust implementation, we might want to:
        // - Try to recover (re-seek, restart decoder, etc.)
        // - Enter a degraded mode (e.g., video-only if audio fails)
        // - Stop completely if too many errors
        
        emitEvent(PlayerEvent::Error, error);
        
        if (consecutiveErrors_ >= kMaxConsecutiveDecoderErrors) {
            spdlog::error("Too many errors, stopping playback");
            pipelineState_ = PipelineState::Error;
            emitEvent(PlayerEvent::Error, "Fatal: too many consecutive errors");
            // Note: We don't automatically stop here to allow graceful shutdown
        }
    }
    
    /**
     * @brief Video decode loop
     */
    void videoDecodeLoop() {
        spdlog::debug("Video decode loop started");
        
        while (isPlaying_) {
            auto [result, packet] = videoDecoder_->input.pop(std::chrono::milliseconds(100));
            
            if (result == PopResult::Terminated) {
                break;
            }
            
            if (result == PopResult::Timeout) {
                continue;
            }
            
            if (!packet.has_value()) {
                continue;
            }
            
            videoDecoder_->process(std::move(*packet));
        }
        
        spdlog::debug("Video decode loop ended");
    }
    
    /**
     * @brief Audio decode loop
     */
    void audioDecodeLoop() {
        spdlog::debug("Audio decode loop started");
        
        while (isPlaying_) {
            auto [result, packet] = audioDecoder_->input.pop(std::chrono::milliseconds(100));
            
            if (result == PopResult::Terminated) {
                break;
            }
            
            if (result == PopResult::Timeout) {
                continue;
            }
            
            if (!packet.has_value()) {
                continue;
            }
            
            audioDecoder_->process(std::move(*packet));
        }
        
        spdlog::debug("Audio decode loop ended");
    }
    
    // Source node
    std::unique_ptr<FileSourceNode> source_;
    
    // Video pipeline nodes
    std::unique_ptr<FFmpegDecodeNode> videoDecoder_;
    std::unique_ptr<VideoSinkNode> videoSink_;
    std::unique_ptr<AsyncQueueNode<Packet>> videoPacketQueue_;
    std::unique_ptr<AsyncQueueNode<VideoFrame>> videoFrameQueue_;
    
    // Audio pipeline nodes
    std::unique_ptr<AudioDecodeNode> audioDecoder_;
    std::unique_ptr<AudioSinkNode> audioSink_;
    std::unique_ptr<AsyncQueueNode<Packet>> audioPacketQueue_;
    std::unique_ptr<AsyncQueueNode<AudioFrame>> audioFrameQueue_;
    
    // Renderer
    RendererPtr renderer_;
    
    // Clock
    MasterClock clock_;
    
    // State
    bool isOpen_ = false;
    bool hasAudio_ = false;
    bool videoReady_ = false;
    bool audioReady_ = false;
    bool clockStarted_ = false;
    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> isPaused_{false};
    std::atomic<bool> eofReached_{false};
    std::atomic<bool> videoEofReached_{false};
    std::atomic<bool> audioEofReached_{false};
    std::atomic<bool> quitRequested_{false};
    std::atomic<uint64_t> seekSerial_{0};
    
    // Pipeline state machine
    PipelineState pipelineState_ = PipelineState::Stopped;
    BufferingState bufferingState_ = BufferingState::Idle;
    TimePoint bufferingStartTime_;
    
    // Event callback
    EventCallback eventCallback_;
    
    // Error tracking
    int consecutiveErrors_ = 0;
    
    // Threads
    std::thread videoDecodeThread_;
    std::thread audioDecodeThread_;
};

void printVersionInfo() {
    spdlog::info("PhoenixEngine v0.1.0");
    spdlog::info("FFmpeg libavcodec version: {}.{}.{}", 
        LIBAVCODEC_VERSION_MAJOR, 
        LIBAVCODEC_VERSION_MINOR,
        LIBAVCODEC_VERSION_MICRO);
    
    SDL_version sdlVersion;
    SDL_GetVersion(&sdlVersion);
    spdlog::info("SDL version: {}.{}.{}", 
        sdlVersion.major, 
        sdlVersion.minor, 
        sdlVersion.patch);
}

void printUsage() {
    spdlog::info("Usage: PhoenixEngine <video_file>");
    spdlog::info("");
    spdlog::info("Controls:");
    spdlog::info("  Space     - Pause/Resume");
    spdlog::info("  Left      - Seek -5 seconds");
    spdlog::info("  Right     - Seek +5 seconds");
    spdlog::info("  Q/ESC     - Quit");
    spdlog::info("  Ctrl+C    - Force quit");
}

int run(int argc, char* argv[]) {
    printVersionInfo();

    if (argc < 2) {
        printUsage();
        return 0;
    }

    const char* videoFile = argv[1];
    spdlog::info("Opening: {}", videoFile);

    // Install signal handlers for graceful shutdown
    installSignalHandlers();

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        spdlog::error("SDL initialization failed: {}", SDL_GetError());
        return 1;
    }

    int exitCode = 0;
    
    // Create and run player
    {
        Player player;
        
        auto result = player.open(videoFile);
        if (!result.ok()) {
            spdlog::error("Failed to open file: {}", result.error().what());
            SDL_Quit();
            return 1;
        }
        
        player.play();
        
        // Main event loop with timeout protection
        auto startTime = std::chrono::steady_clock::now();
        const auto maxRunTime = std::chrono::hours(24);  // 24 hour safety timeout
        
        while (player.processEvents()) {
            // Check pre-roll timeout
            player.checkPrerollTimeout();
            
            // Check if playback finished
            if (!player.isPlaying()) {
                spdlog::info("Playback finished");
                break;
            }
            
            // Check for quit request
            if (player.isQuitRequested()) {
                spdlog::info("Quit requested, exiting...");
                break;
            }
            
            // Safety timeout check (prevent infinite loop)
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > maxRunTime) {
                spdlog::warn("Maximum runtime exceeded, forcing exit");
                exitCode = 2;
                break;
            }
            
            SDL_Delay(10);  // Prevent busy loop
        }
        
        // Graceful shutdown
        spdlog::info("Initiating cleanup...");
        player.shutdown();
    }

    // Cleanup SDL
    spdlog::info("Shutting down SDL...");
    SDL_Quit();
    
    if (exitCode == 0) {
        spdlog::info("Shutdown complete. Goodbye!");
    } else {
        spdlog::warn("Exited with code: {}", exitCode);
    }

    return exitCode;
}

} // namespace phoenix

int main(int argc, char* argv[]) {
    try {
        // Configure logging
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        
        return phoenix::run(argc, argv);
    } catch (const std::exception& e) {
        spdlog::critical("Unhandled exception: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::critical("Unknown exception");
        return 1;
    }
}
