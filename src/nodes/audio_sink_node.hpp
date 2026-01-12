/**
 * @file audio_sink_node.hpp
 * @brief AudioSinkNode - Audio output via SDL with A/V sync
 * 
 * Key features:
 * - SwrContext for format conversion (any format -> S16 stereo)
 * - Lock-free ring buffer for SDL callback
 * - Audio callback drives MasterClock
 * - Pause outputs silence without updating clock
 * - Serial filtering for seek support
 * 
 * CRITICAL RULES:
 * - NO mutex in SDL callback (use lock-free ring buffer)
 * - Check pipeline state atomically in callback
 * - Only update clock when actually playing audio
 */

#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <cstring>

#include "graph/node.hpp"
#include "core/types.hpp"
#include "core/media_frame.hpp"
#include "core/clock.hpp"
#include "core/ring_buffer.hpp"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

namespace phoenix {

/// Target audio format for SDL output
constexpr int kOutputSampleRate = 48000;
constexpr int kOutputChannels = 2;
constexpr int kOutputBytesPerSample = 2;  // 16-bit
constexpr SDL_AudioFormat kOutputFormat = AUDIO_S16SYS;

/// Ring buffer size: ~500ms of audio
constexpr size_t kAudioRingBufferSize = 
    kOutputSampleRate * kOutputChannels * kOutputBytesPerSample / 2;  // 96000 bytes

/// SDL audio buffer size (samples per callback)
constexpr int kSDLAudioBufferSamples = 2048;

/**
 * @brief SDL audio callback wrapper
 * Forward declaration for friend access
 */
class AudioSinkNode;
void sdlAudioCallback(void* userdata, Uint8* stream, int len);

/**
 * @brief Audio sink node with SDL output
 * 
 * Consumes AudioFrames, resamples to output format,
 * and plays via SDL audio subsystem.
 */
class AudioSinkNode : public NodeBase {
public:
    explicit AudioSinkNode(std::string name = "AudioSink",
                          size_t inputCapacity = kDefaultAudioQueueCapacity)
        : NodeBase(std::move(name))
        , input(inputCapacity)
        , ringBuffer_(kAudioRingBufferSize)
    {}
    
    ~AudioSinkNode() override {
        stop();
        closeAudioDevice();
        destroyResampler();
        av_channel_layout_uninit(&srcChannelLayout_);
    }
    
    /// Input pin for audio frames
    InputPin<AudioFrame> input;
    
    // ========== Configuration ==========
    
    /**
     * @brief Set the master clock (audio drives sync)
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
     * @brief Set pause state (for silence output)
     */
    void setPaused(bool paused) {
        paused_.store(paused, std::memory_order_release);
        if (audioDeviceId_ != 0) {
            SDL_PauseAudioDevice(audioDeviceId_, paused ? 1 : 0);
        }
    }
    
    /**
     * @brief Set EOF callback
     */
    void setEofCallback(std::function<void()> cb) { 
        eofCallback_ = std::move(cb); 
    }
    
    /**
     * @brief Set ready callback (first audio played)
     */
    void setReadyCallback(std::function<void()> cb) {
        readyCallback_ = std::move(cb);
    }
    
    /**
     * @brief Set error callback
     */
    void setErrorCallback(std::function<void(const std::string&)> cb) {
        errorCallback_ = std::move(cb);
    }
    
    // ========== Initialization ==========
    
    /**
     * @brief Initialize audio output
     * @param codecCtx Source codec context for format info
     * @return Ok on success, error otherwise
     */
    Result<void> init(AVCodecContext* codecCtx) {
        if (!codecCtx) {
            return Err(ErrorCode::InvalidArgument, "Null codec context");
        }
        
        // Store source format info
        srcSampleRate_ = codecCtx->sample_rate;
        srcFormat_ = codecCtx->sample_fmt;
        
        // CRITICAL: Use av_channel_layout_copy for proper deep copy
        int ret = av_channel_layout_copy(&srcChannelLayout_, &codecCtx->ch_layout);
        if (ret < 0) {
            return Err(ErrorCode::OutOfMemory, "Failed to copy channel layout");
        }
        
        spdlog::info("[{}] Source audio: {} Hz, {} channels, format {}",
            name_, srcSampleRate_, srcChannelLayout_.nb_channels,
            av_get_sample_fmt_name(srcFormat_));
        
        // Create resampler
        auto result = createResampler();
        if (!result.ok()) {
            av_channel_layout_uninit(&srcChannelLayout_);
            return result;
        }
        
        // Open SDL audio device
        result = openAudioDevice();
        if (!result.ok()) {
            destroyResampler();
            av_channel_layout_uninit(&srcChannelLayout_);
            return result;
        }
        
        spdlog::info("[{}] Output audio: {} Hz, {} channels, S16",
            name_, kOutputSampleRate, kOutputChannels);
        
        return Ok();
    }
    
    // ========== Lifecycle ==========
    
    void start() override {
        if (running_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        
        firstAudio_ = true;
        eofReceived_ = false;
        input.reset();
        ringBuffer_.clear();
        
        // Start worker thread
        worker_ = std::thread([this] { workerLoop(); });
        
        // Start audio playback
        if (audioDeviceId_ != 0) {
            SDL_PauseAudioDevice(audioDeviceId_, 0);  // Unpause
        }
        
        spdlog::info("[{}] Started", name_);
    }
    
    void stop() override {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        
        // Stop audio playback
        if (audioDeviceId_ != 0) {
            SDL_PauseAudioDevice(audioDeviceId_, 1);  // Pause
        }
        
        input.stop();
        
        if (worker_.joinable()) {
            worker_.join();
        }
        
        spdlog::info("[{}] Stopped. Samples written: {}", 
            name_, samplesWritten_.load());
    }
    
    void flush() override {
        input.flush();
        ringBuffer_.clear();
        firstAudio_ = true;
        spdlog::debug("[{}] Flushed", name_);
    }
    
    // ========== Audio Callback Interface ==========
    
    /**
     * @brief Called from SDL audio callback
     * 
     * CRITICAL: No mutex, no memory allocation, no logging!
     */
    void audioCallback(Uint8* stream, int len) {
        // Check if paused - output silence
        if (paused_.load(std::memory_order_acquire)) {
            std::memset(stream, 0, len);
            return;
        }
        
        // Check if running
        if (!running_.load(std::memory_order_acquire)) {
            std::memset(stream, 0, len);
            return;
        }
        
        // Read from ring buffer
        size_t read = ringBuffer_.read(stream, static_cast<size_t>(len));
        
        // Fill remainder with silence if buffer underrun
        if (read < static_cast<size_t>(len)) {
            std::memset(stream + read, 0, len - read);
        }
        
        // Update master clock based on audio position
        if (read > 0 && clock_) {
            // Get current audio PTS and update clock
            Timestamp currentTime = currentAudioPts_.load(std::memory_order_acquire);
            if (currentTime != kNoTimestamp) {
                // Calculate elapsed time based on bytes played
                // bytes = samples * channels * bytesPerSample
                // time = samples / sampleRate
                size_t samples = read / (kOutputChannels * kOutputBytesPerSample);
                int64_t elapsedUs = static_cast<int64_t>(samples) * 1000000 / kOutputSampleRate;
                
                // Update clock with current audio position plus elapsed time
                clock_->update(currentTime + elapsedUs);
            }
        }
    }
    
    // ========== Statistics ==========
    
    [[nodiscard]] uint64_t samplesWritten() const {
        return samplesWritten_.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] float bufferFillRatio() const {
        return ringBuffer_.fillRatio();
    }
    
private:
    /**
     * @brief Create SwrContext for resampling
     */
    Result<void> createResampler() {
        // Allocate resampler
        swrCtx_ = swr_alloc();
        if (!swrCtx_) {
            return Err(ErrorCode::OutOfMemory, "Failed to allocate SwrContext");
        }
        
        // Set output channel layout (stereo)
        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, kOutputChannels);
        
        // Configure resampler using new API
        int ret = swr_alloc_set_opts2(
            &swrCtx_,
            &outLayout,                             // Output layout
            AV_SAMPLE_FMT_S16,                      // Output format
            kOutputSampleRate,                       // Output sample rate
            &srcChannelLayout_,                      // Input layout
            srcFormat_,                              // Input format
            srcSampleRate_,                          // Input sample rate
            0,                                       // Log offset
            nullptr                                  // Log context
        );
        
        if (ret < 0) {
            swr_free(&swrCtx_);
            return Err(ffmpegError(ret, "swr_alloc_set_opts2"));
        }
        
        // Initialize resampler
        ret = swr_init(swrCtx_);
        if (ret < 0) {
            swr_free(&swrCtx_);
            return Err(ffmpegError(ret, "swr_init"));
        }
        
        spdlog::debug("[{}] Resampler created", name_);
        return Ok();
    }
    
    /**
     * @brief Destroy resampler
     */
    void destroyResampler() {
        if (swrCtx_) {
            swr_free(&swrCtx_);
            swrCtx_ = nullptr;
        }
    }
    
    /**
     * @brief Open SDL audio device
     */
    Result<void> openAudioDevice() {
        SDL_AudioSpec wanted, obtained;
        
        SDL_zero(wanted);
        wanted.freq = kOutputSampleRate;
        wanted.format = kOutputFormat;
        wanted.channels = kOutputChannels;
        wanted.samples = kSDLAudioBufferSamples;
        wanted.callback = sdlAudioCallback;
        wanted.userdata = this;
        
        audioDeviceId_ = SDL_OpenAudioDevice(
            nullptr,    // Default device
            0,          // Playback (not capture)
            &wanted,
            &obtained,
            0           // No allowed changes
        );
        
        if (audioDeviceId_ == 0) {
            return Err(ErrorCode::DeviceError, 
                std::format("Failed to open audio device: {}", SDL_GetError()));
        }
        
        spdlog::debug("[{}] Opened audio device {}: {} Hz, {} channels, {} samples/buffer",
            name_, audioDeviceId_, obtained.freq, obtained.channels, obtained.samples);
        
        return Ok();
    }
    
    /**
     * @brief Close SDL audio device
     */
    void closeAudioDevice() {
        if (audioDeviceId_ != 0) {
            SDL_CloseAudioDevice(audioDeviceId_);
            audioDeviceId_ = 0;
        }
    }
    
    /**
     * @brief Worker thread loop
     */
    void workerLoop() {
        spdlog::debug("[{}] Worker started", name_);
        
        // Temporary buffer for resampled audio
        std::vector<uint8_t> resampleBuffer;
        resampleBuffer.reserve(32768);  // 32KB initial
        
        while (running_.load(std::memory_order_acquire)) {
            // If paused, just wait without processing frames
            // This prevents ring buffer overflow when SDL callback is paused
            if (paused_.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Pop frame from input
            auto [result, frame] = input.pop(std::chrono::milliseconds(100));
            
            if (result == PopResult::Terminated) {
                break;
            }
            
            if (result == PopResult::Timeout) {
                continue;
            }
            
            if (!frame.has_value()) {
                continue;
            }
            
            // Process the frame
            processFrame(std::move(*frame), resampleBuffer);
        }
        
        spdlog::debug("[{}] Worker exited", name_);
    }
    
    /**
     * @brief Process an audio frame
     */
    void processFrame(AudioFrame frame, std::vector<uint8_t>& outBuffer) {
        // Check for EOF
        if (frame.isEof()) {
            spdlog::info("[{}] Received EOF", name_);
            eofReceived_ = true;
            if (eofCallback_) {
                eofCallback_();
            }
            return;
        }
        
        // Check for error frame
        if (frame.isError()) {
            consecutiveErrors_++;
            spdlog::error("[{}] Received error frame (consecutive: {})", name_, consecutiveErrors_);
            
            if (consecutiveErrors_ >= kMaxConsecutiveDecoderErrors && errorCallback_) {
                errorCallback_("Too many consecutive audio decode errors");
            }
            return;
        }
        
        // Reset error count on valid frame
        consecutiveErrors_ = 0;
        
        // Filter stale frames
        uint64_t expectedSerial = currentSerial_.load(std::memory_order_acquire);
        if (frame.serial != expectedSerial) {
            spdlog::trace("[{}] Dropping stale frame (serial {} != {})", 
                name_, frame.serial, expectedSerial);
            return;
        }
        
        // First audio notification
        if (firstAudio_) {
            firstAudio_ = false;
            spdlog::info("[{}] First audio frame received: pts={}, samples={}, rate={}, channels={}", 
                name_, frame.pts, frame.nbSamples, frame.sampleRate, frame.channels);
            
            if (readyCallback_) {
                readyCallback_();
            }
        }
        
        // Update current PTS for clock
        currentAudioPts_.store(frame.pts, std::memory_order_release);
        
        // Resample and write to ring buffer
        if (!frame.hasData()) {
            spdlog::warn("[{}] Frame has no data", name_);
            return;
        }
        
        if (!swrCtx_) {
            spdlog::error("[{}] No resampler context", name_);
            return;
        }
        
        AVFrame* avFrame = frame.avFrame.get();
        if (!avFrame) {
            spdlog::warn("[{}] Null AVFrame", name_);
            return;
        }
        
        // Calculate output buffer size
        int outSamples = swr_get_out_samples(swrCtx_, avFrame->nb_samples);
        if (outSamples <= 0) {
            spdlog::warn("[{}] swr_get_out_samples returned {}", name_, outSamples);
            return;
        }
        
        size_t outBufferSize = outSamples * kOutputChannels * kOutputBytesPerSample;
        outBuffer.resize(outBufferSize);
        
        uint8_t* outPtr = outBuffer.data();
        
        // Resample
        int samplesConverted = swr_convert(
            swrCtx_,
            &outPtr,
            outSamples,
            const_cast<const uint8_t**>(avFrame->data),
            avFrame->nb_samples
        );
        
        if (samplesConverted < 0) {
            spdlog::warn("[{}] Resample error: {}", name_, ffmpegErrorString(samplesConverted));
            return;
        }
        
        if (samplesConverted > 0) {
            size_t bytesToWrite = samplesConverted * kOutputChannels * kOutputBytesPerSample;
            
            // Write to ring buffer (non-blocking when stopping)
            size_t written = 0;
            int spinCount = 0;
            constexpr int kMaxSpinCount = 50;  // Reduced from 100
            
            while (written < bytesToWrite && running_.load(std::memory_order_acquire)) {
                size_t n = ringBuffer_.write(outBuffer.data() + written, bytesToWrite - written);
                written += n;
                
                if (n == 0) {
                    // Buffer full, wait a bit but check running_ frequently
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    if (++spinCount > kMaxSpinCount) {
                        // Don't log if we're shutting down
                        if (running_.load(std::memory_order_acquire)) {
                            spdlog::warn("[{}] Ring buffer write timeout", name_);
                        }
                        break;
                    }
                }
            }
            
            // Only update stats if we actually wrote something
            if (written > 0) {
                uint64_t totalSamples = samplesWritten_.fetch_add(samplesConverted, std::memory_order_relaxed);
                
                // Log progress periodically (only if still running)
                if (running_.load(std::memory_order_relaxed)) {
                    if ((totalSamples / kOutputSampleRate) != ((totalSamples + samplesConverted) / kOutputSampleRate)) {
                        spdlog::debug("[{}] Audio: {} seconds played, buffer fill: {:.1f}%", 
                            name_, (totalSamples + samplesConverted) / kOutputSampleRate,
                            ringBuffer_.fillRatio() * 100.0f);
                    }
                }
            }
        }
    }
    
    // Dependencies
    MasterClock* clock_ = nullptr;
    
    // Serial for seek filtering
    std::atomic<uint64_t> currentSerial_{0};
    
    // State
    bool firstAudio_ = true;
    bool eofReceived_ = false;
    std::atomic<bool> paused_{false};
    
    // Audio PTS for clock updates
    std::atomic<Timestamp> currentAudioPts_{kNoTimestamp};
    
    // Callbacks
    std::function<void()> eofCallback_;
    std::function<void()> readyCallback_;
    std::function<void(const std::string&)> errorCallback_;
    
    // Error tracking
    int consecutiveErrors_ = 0;
    
    // Ring buffer
    LockFreeRingBuffer ringBuffer_;
    
    // SDL audio
    SDL_AudioDeviceID audioDeviceId_ = 0;
    
    // Resampler
    SwrContext* swrCtx_ = nullptr;
    int srcSampleRate_ = 0;
    AVSampleFormat srcFormat_ = AV_SAMPLE_FMT_NONE;
    AVChannelLayout srcChannelLayout_{};
    
    // Worker thread
    std::thread worker_;
    
    // Statistics
    std::atomic<uint64_t> samplesWritten_{0};
    
    // Friend for SDL callback
    friend void sdlAudioCallback(void* userdata, Uint8* stream, int len);
};

/**
 * @brief SDL audio callback function
 * 
 * CRITICAL: This runs on audio thread - no blocking, no allocation!
 */
inline void sdlAudioCallback(void* userdata, Uint8* stream, int len) {
    auto* sink = static_cast<AudioSinkNode*>(userdata);
    sink->audioCallback(stream, len);
}

} // namespace phoenix

