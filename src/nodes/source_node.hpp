/**
 * @file source_node.hpp
 * @brief FileSourceNode - Demuxes media files using FFmpeg
 * 
 * Reads packets from media container and routes to video/audio outputs.
 * Supports pause/resume/seek operations.
 * 
 * Key features:
 * - Converts timestamps to microseconds at source boundary
 * - Tags packets with serial number for seek atomicity
 * - Handles EOF by emitting EOF packets
 */

#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "graph/node.hpp"
#include "graph/pin.hpp"
#include "core/types.hpp"
#include "core/media_frame.hpp"
#include "codec/ffmpeg/ff_utils.hpp"

#include <spdlog/spdlog.h>

namespace phoenix {

/**
 * @brief File source node that demuxes media files
 * 
 * Opens a media file and reads packets in a background thread.
 * Routes video and audio packets to separate output pins.
 */
class FileSourceNode : public NodeBase {
public:
    explicit FileSourceNode(std::string name = "FileSource")
        : NodeBase(std::move(name))
    {}
    
    ~FileSourceNode() override {
        stop();
    }
    
    // Non-copyable, non-movable
    FileSourceNode(const FileSourceNode&) = delete;
    FileSourceNode& operator=(const FileSourceNode&) = delete;
    
    /// Output pin for video packets
    OutputPin<Packet> videoOutput;
    
    /// Output pin for audio packets (Phase 2)
    OutputPin<Packet> audioOutput;
    
    // ========== File Operations ==========
    
    /**
     * @brief Open a media file
     * @param filename Path to media file
     * @return Ok on success, error otherwise
     */
    Result<void> open(const std::string& filename) {
        std::lock_guard lock(mutex_);
        
        // Close any existing file
        closeInternal();
        
        // Open input
        auto result = openInputFile(filename);
        if (!result.ok()) {
            return Err(result.error());
        }
        formatCtx_ = std::move(result.value());
        filename_ = filename;
        
        // Find streams
        videoStreamIndex_ = findBestStream(formatCtx_.get(), AVMEDIA_TYPE_VIDEO);
        audioStreamIndex_ = findBestStream(formatCtx_.get(), AVMEDIA_TYPE_AUDIO);
        
        if (videoStreamIndex_ < 0 && audioStreamIndex_ < 0) {
            closeInternal();
            return Err(ErrorCode::NotFound, "No video or audio stream found");
        }
        
        // Log stream info
        if (videoStreamIndex_ >= 0) {
            auto* stream = formatCtx_->streams[videoStreamIndex_];
            spdlog::info("[{}] Video stream {}: {}x{}, {} fps",
                name_, videoStreamIndex_,
                stream->codecpar->width, stream->codecpar->height,
                av_q2d(stream->avg_frame_rate));
        }
        
        if (audioStreamIndex_ >= 0) {
            auto* stream = formatCtx_->streams[audioStreamIndex_];
            spdlog::info("[{}] Audio stream {}: {} Hz, {} channels",
                name_, audioStreamIndex_,
                stream->codecpar->sample_rate,
                stream->codecpar->ch_layout.nb_channels);
        }
        
        // Get duration
        if (formatCtx_->duration != AV_NOPTS_VALUE) {
            durationUs_ = av_rescale_q(formatCtx_->duration, 
                AV_TIME_BASE_Q, AVRational{1, 1000000});
            spdlog::info("[{}] Duration: {:.2f} seconds", 
                name_, durationUs_ / 1000000.0);
        }
        
        isOpen_ = true;
        return Ok();
    }
    
    /**
     * @brief Close the media file
     */
    void close() {
        stop();
        std::lock_guard lock(mutex_);
        closeInternal();
    }
    
    /**
     * @brief Check if file is open
     */
    [[nodiscard]] bool isOpen() const {
        std::lock_guard lock(mutex_);
        return isOpen_;
    }
    
    // ========== Stream Info ==========
    
    [[nodiscard]] int videoStreamIndex() const { return videoStreamIndex_; }
    [[nodiscard]] int audioStreamIndex() const { return audioStreamIndex_; }
    [[nodiscard]] bool hasVideoStream() const { return videoStreamIndex_ >= 0; }
    [[nodiscard]] bool hasAudioStream() const { return audioStreamIndex_ >= 0; }
    [[nodiscard]] Duration duration() const { return durationUs_; }
    
    /**
     * @brief Get video stream for decoder creation
     */
    AVStream* getVideoStream() const {
        if (videoStreamIndex_ < 0 || !formatCtx_) return nullptr;
        return formatCtx_->streams[videoStreamIndex_];
    }
    
    /**
     * @brief Get audio stream for decoder creation
     */
    AVStream* getAudioStream() const {
        if (audioStreamIndex_ < 0 || !formatCtx_) return nullptr;
        return formatCtx_->streams[audioStreamIndex_];
    }
    
    // ========== Lifecycle ==========
    
    void start() override {
        if (!isOpen_) {
            spdlog::error("[{}] Cannot start: no file open", name_);
            return;
        }
        
        if (running_.exchange(true, std::memory_order_acq_rel)) {
            return;  // Already running
        }
        
        paused_.store(false, std::memory_order_release);
        
        // Start read thread
        readThread_ = std::thread([this] { readLoop(); });
        
        spdlog::info("[{}] Started", name_);
    }
    
    void stop() override {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;  // Already stopped
        }
        
        // Wake paused thread
        {
            std::lock_guard lock(pauseMutex_);
            paused_.store(false, std::memory_order_release);
        }
        pauseCv_.notify_all();
        
        // Join read thread
        if (readThread_.joinable()) {
            readThread_.join();
        }
        
        spdlog::info("[{}] Stopped", name_);
    }
    
    void flush() override {
        // Nothing to flush in source - packets are read fresh
    }
    
    // ========== Playback Control ==========
    
    /**
     * @brief Pause reading
     */
    void pause() {
        paused_.store(true, std::memory_order_release);
        spdlog::debug("[{}] Paused", name_);
    }
    
    /**
     * @brief Resume reading
     */
    void resume() {
        {
            std::lock_guard lock(pauseMutex_);
            paused_.store(false, std::memory_order_release);
        }
        pauseCv_.notify_all();
        spdlog::debug("[{}] Resumed", name_);
    }
    
    /**
     * @brief Seek to position
     * @param positionUs Target position in microseconds
     */
    Result<void> seekTo(Timestamp positionUs) {
        std::lock_guard lock(mutex_);
        
        if (!formatCtx_) {
            return Err(ErrorCode::InvalidArgument, "No file open");
        }
        
        // Convert to AV_TIME_BASE
        int64_t timestamp = av_rescale_q(positionUs, 
            AVRational{1, 1000000}, AV_TIME_BASE_Q);
        
        int ret = av_seek_frame(formatCtx_.get(), -1, timestamp, 
            AVSEEK_FLAG_BACKWARD);
        
        if (ret < 0) {
            return Err(ffmpegError(ret, "av_seek_frame"));
        }
        
        spdlog::debug("[{}] Seeked to {} us", name_, positionUs);
        return Ok();
    }
    
    /**
     * @brief Set serial number for seek atomicity
     */
    void setSerial(uint64_t serial) {
        currentSerial_.store(serial, std::memory_order_release);
    }
    
    /**
     * @brief Get current serial number
     */
    [[nodiscard]] uint64_t serial() const {
        return currentSerial_.load(std::memory_order_acquire);
    }
    
private:
    void closeInternal() {
        formatCtx_.reset();
        filename_.clear();
        videoStreamIndex_ = -1;
        audioStreamIndex_ = -1;
        durationUs_ = 0;
        isOpen_ = false;
    }
    
    /**
     * @brief Main read loop
     */
    void readLoop() {
        spdlog::debug("[{}] Read loop started", name_);
        
        PacketWrapper pkt;
        
        while (running_.load(std::memory_order_acquire)) {
            // Handle pause
            {
                std::unique_lock lock(pauseMutex_);
                pauseCv_.wait(lock, [this] {
                    return !paused_.load(std::memory_order_acquire) || 
                           !running_.load(std::memory_order_acquire);
                });
            }
            
            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
            
            // Read packet
            int ret;
            {
                std::lock_guard lock(mutex_);
                ret = av_read_frame(formatCtx_.get(), pkt.get());
            }
            
            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    // Send EOF to all outputs
                    sendEof();
                    spdlog::info("[{}] Reached end of file", name_);
                } else {
                    spdlog::error("[{}] Read error: {}", name_, ffmpegErrorString(ret));
                }
                break;
            }
            
            // Convert and route packet
            int streamIndex = pkt->stream_index;
            
            if (streamIndex == videoStreamIndex_) {
                auto packet = convertPacket(pkt.get(), 
                    formatCtx_->streams[videoStreamIndex_]);
                packet.mediaType = MediaType::Video;
                
                if (videoOutput.isConnected()) {
                    auto result = videoOutput.emit(std::move(packet));
                    if (!result.ok() && running_.load(std::memory_order_acquire)) {
                        spdlog::warn("[{}] Failed to emit video packet", name_);
                    }
                }
            } else if (streamIndex == audioStreamIndex_) {
                auto packet = convertPacket(pkt.get(), 
                    formatCtx_->streams[audioStreamIndex_]);
                packet.mediaType = MediaType::Audio;
                
                if (audioOutput.isConnected()) {
                    auto result = audioOutput.emit(std::move(packet));
                    if (!result.ok() && running_.load(std::memory_order_acquire)) {
                        spdlog::warn("[{}] Failed to emit audio packet", name_);
                    }
                }
            }
            // Ignore other streams
            
            pkt.unref();
        }
        
        spdlog::debug("[{}] Read loop ended", name_);
    }
    
    /**
     * @brief Convert AVPacket to our Packet type
     */
    Packet convertPacket(AVPacket* avpkt, AVStream* stream) {
        Packet packet;
        
        // Copy data
        packet.data.assign(avpkt->data, avpkt->data + avpkt->size);
        
        // Convert timestamps to microseconds
        packet.pts = ptsToMicroseconds(avpkt->pts, stream->time_base);
        packet.dts = ptsToMicroseconds(avpkt->dts, stream->time_base);
        packet.duration = durationToMicroseconds(avpkt->duration, stream->time_base);
        
        // Other fields
        packet.streamIndex = avpkt->stream_index;
        packet.keyFrame = (avpkt->flags & AV_PKT_FLAG_KEY) != 0;
        packet.serial = currentSerial_.load(std::memory_order_acquire);
        
        return packet;
    }
    
    /**
     * @brief Send EOF packets to all connected outputs
     */
    void sendEof() {
        uint64_t serial = currentSerial_.load(std::memory_order_acquire);
        
        if (videoOutput.isConnected()) {
            videoOutput.emit(Packet::eof(serial));
        }
        if (audioOutput.isConnected()) {
            audioOutput.emit(Packet::eof(serial));
        }
    }
    
    // File state
    mutable std::mutex mutex_;
    FormatContextPtr formatCtx_;
    std::string filename_;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    Duration durationUs_ = 0;
    bool isOpen_ = false;
    
    // Serial number for seek atomicity
    std::atomic<uint64_t> currentSerial_{0};
    
    // Pause control
    std::atomic<bool> paused_{false};
    std::mutex pauseMutex_;
    std::condition_variable pauseCv_;
    
    // Read thread
    std::thread readThread_;
};

} // namespace phoenix

