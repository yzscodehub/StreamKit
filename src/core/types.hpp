/**
 * @file types.hpp
 * @brief Core type definitions for PhoenixEngine
 * 
 * All internal timestamps use int64_t microseconds to prevent
 * floating-point precision loss during long playback sessions.
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <string>

namespace phoenix {

// ============================================================================
// Time Types - ALL timestamps are microseconds (int64_t)
// ============================================================================

/// Timestamp in microseconds since media start
using Timestamp = int64_t;

/// Duration in microseconds
using Duration = int64_t;

/// Time base constant: 1 second = 1,000,000 microseconds
constexpr Timestamp kTimeBaseUs = 1'000'000;

/// Invalid timestamp sentinel
constexpr Timestamp kNoTimestamp = INT64_MIN;

/// std::chrono duration type for microseconds
using Microseconds = std::chrono::microseconds;
using Milliseconds = std::chrono::milliseconds;
using Nanoseconds = std::chrono::nanoseconds;

/// High-resolution clock
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// ============================================================================
// Media Types
// ============================================================================

/// Pixel format enumeration (matches common FFmpeg formats)
enum class PixelFormat {
    Unknown = 0,
    YUV420P,    // Planar YUV 4:2:0, 12bpp
    YUV422P,    // Planar YUV 4:2:2, 16bpp
    YUV444P,    // Planar YUV 4:4:4, 24bpp
    NV12,       // Semi-planar Y + interleaved UV
    NV21,       // Semi-planar Y + interleaved VU
    RGB24,      // Packed RGB 8:8:8, 24bpp
    RGBA,       // Packed RGBA 8:8:8:8, 32bpp
    BGRA,       // Packed BGRA 8:8:8:8, 32bpp
    // Hardware formats
    D3D11,      // D3D11 texture (Windows)
    CUDA,       // CUDA device memory
    VAAPI,      // VAAPI surface (Linux)
    VideoToolbox, // VideoToolbox CVPixelBuffer (macOS)
};

/// Sample format for audio
enum class SampleFormat {
    Unknown = 0,
    U8,         // Unsigned 8-bit
    S16,        // Signed 16-bit
    S32,        // Signed 32-bit
    Float,      // 32-bit float
    Double,     // 64-bit float
    // Planar formats
    U8P,
    S16P,
    S32P,
    FloatP,
    DoubleP,
};

/// Media type enumeration
enum class MediaType {
    Unknown = 0,
    Video,
    Audio,
    Subtitle,
    Data,
};

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    Ok = 0,
    
    // General errors
    Unknown,
    InvalidArgument,
    NotFound,
    NotSupported,
    OutOfMemory,
    
    // I/O errors
    FileNotFound,
    FileOpenFailed,
    ReadError,
    WriteError,
    EndOfFile,
    
    // Codec errors
    CodecNotFound,
    CodecOpenFailed,
    DecoderError,
    EncoderError,
    InvalidData,
    
    // Pipeline errors
    Timeout,
    Terminated,
    QueueFull,
    QueueEmpty,
    
    // Render errors
    RenderError,
    WindowCreationFailed,
    TextureCreationFailed,
    
    // Device errors
    DeviceError,
    AudioDeviceError,
};

/// Convert ErrorCode to string
inline const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "Ok";
        case ErrorCode::Unknown: return "Unknown error";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::NotFound: return "Not found";
        case ErrorCode::NotSupported: return "Not supported";
        case ErrorCode::OutOfMemory: return "Out of memory";
        case ErrorCode::FileNotFound: return "File not found";
        case ErrorCode::FileOpenFailed: return "File open failed";
        case ErrorCode::ReadError: return "Read error";
        case ErrorCode::WriteError: return "Write error";
        case ErrorCode::EndOfFile: return "End of file";
        case ErrorCode::CodecNotFound: return "Codec not found";
        case ErrorCode::CodecOpenFailed: return "Codec open failed";
        case ErrorCode::DecoderError: return "Decoder error";
        case ErrorCode::EncoderError: return "Encoder error";
        case ErrorCode::InvalidData: return "Invalid data";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::Terminated: return "Terminated";
        case ErrorCode::QueueFull: return "Queue full";
        case ErrorCode::QueueEmpty: return "Queue empty";
        case ErrorCode::RenderError: return "Render error";
        case ErrorCode::WindowCreationFailed: return "Window creation failed";
        case ErrorCode::TextureCreationFailed: return "Texture creation failed";
        case ErrorCode::DeviceError: return "Device error";
        case ErrorCode::AudioDeviceError: return "Audio device error";
        default: return "Unknown error code";
    }
}

// ============================================================================
// Pipeline State
// ============================================================================

enum class PipelineState {
    Stopped,        // Initial state, not running
    Buffering,      // Waiting for both A/V to be ready (pre-roll)
    Prerolled,      // Both ready, about to start
    Playing,        // Normal playback
    Paused,         // User paused
    Seeking,        // Seek in progress
    Error,          // Fatal error occurred
};

/// Convert PipelineState to string
inline const char* pipelineStateToString(PipelineState state) {
    switch (state) {
        case PipelineState::Stopped: return "Stopped";
        case PipelineState::Buffering: return "Buffering";
        case PipelineState::Prerolled: return "Prerolled";
        case PipelineState::Playing: return "Playing";
        case PipelineState::Paused: return "Paused";
        case PipelineState::Seeking: return "Seeking";
        case PipelineState::Error: return "Error";
        default: return "Unknown";
    }
}

// ============================================================================
// A/V Sync Actions
// ============================================================================

enum class SyncAction {
    Present,    // Render immediately
    Wait,       // Video too fast, need to wait
    Drop,       // Video too late, drop frame
};

// ============================================================================
// Utility Constants
// ============================================================================

/// Default queue capacity for video packets (30-50 recommended)
constexpr size_t kDefaultVideoQueueCapacity = 50;

/// Default queue capacity for audio packets (500-1000 recommended)
constexpr size_t kDefaultAudioQueueCapacity = 1000;

/// Pre-roll timeout in milliseconds
constexpr int64_t kPrerollTimeoutMs = 500;

/// Maximum consecutive decoder errors before failure
constexpr int kMaxConsecutiveDecoderErrors = 10;

/// Maximum decode loop iterations (safety limit)
constexpr int kMaxDecodeLoopIterations = 100;

// ============================================================================
// Buffering State (for Pre-roll State Machine)
// ============================================================================

/**
 * @brief Detailed buffering state for pre-roll tracking
 */
enum class BufferingState : uint8_t {
    Idle,           // Not buffering
    WaitingVideo,   // Waiting for first video frame only
    WaitingAudio,   // Waiting for first audio frame only
    WaitingBoth,    // Waiting for both streams
    Ready,          // All required streams ready
    Timeout         // Timeout occurred, using fallback (wall clock)
};

/// Convert BufferingState to string
inline const char* bufferingStateToString(BufferingState state) {
    switch (state) {
        case BufferingState::Idle: return "Idle";
        case BufferingState::WaitingVideo: return "WaitingVideo";
        case BufferingState::WaitingAudio: return "WaitingAudio";
        case BufferingState::WaitingBoth: return "WaitingBoth";
        case BufferingState::Ready: return "Ready";
        case BufferingState::Timeout: return "Timeout";
        default: return "Unknown";
    }
}

// ============================================================================
// Player Events (for UI Callbacks)
// ============================================================================

/**
 * @brief Event types for UI notification
 */
enum class PlayerEvent : uint8_t {
    None,
    Ready,              // Playback ready to start
    Playing,            // Playback started
    Paused,             // Playback paused
    Stopped,            // Playback stopped
    Seeking,            // Seek started
    SeekComplete,       // Seek completed
    EndOfFile,          // Reached end of file
    Buffering,          // Buffering in progress
    BufferingComplete,  // Buffering complete
    Error,              // Error occurred
    Warning             // Non-fatal warning
};

/// Convert PlayerEvent to string
inline const char* playerEventToString(PlayerEvent event) {
    switch (event) {
        case PlayerEvent::None: return "None";
        case PlayerEvent::Ready: return "Ready";
        case PlayerEvent::Playing: return "Playing";
        case PlayerEvent::Paused: return "Paused";
        case PlayerEvent::Stopped: return "Stopped";
        case PlayerEvent::Seeking: return "Seeking";
        case PlayerEvent::SeekComplete: return "SeekComplete";
        case PlayerEvent::EndOfFile: return "EndOfFile";
        case PlayerEvent::Buffering: return "Buffering";
        case PlayerEvent::BufferingComplete: return "BufferingComplete";
        case PlayerEvent::Error: return "Error";
        case PlayerEvent::Warning: return "Warning";
        default: return "Unknown";
    }
}

/// A/V sync thresholds in microseconds
constexpr Duration kSyncWaitThreshold = 500'000;   // 500ms - video too fast
constexpr Duration kSyncDropThreshold = -100'000;  // -100ms - video too late
constexpr Duration kSyncRushThreshold = -40'000;   // -40ms - slightly late

} // namespace phoenix

