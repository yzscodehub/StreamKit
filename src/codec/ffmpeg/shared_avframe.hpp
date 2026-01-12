/**
 * @file shared_avframe.hpp
 * @brief Reference-counted AVFrame wrapper using FFmpeg's internal refcount
 * 
 * IMPORTANT: Do NOT use std::shared_ptr for AVFrame!
 * FFmpeg provides built-in reference counting via av_frame_ref/av_frame_unref.
 * This class leverages that mechanism for zero-copy frame sharing.
 */

#pragma once

extern "C" {
#include <libavutil/frame.h>
}

#include <utility>

namespace phoenix {

/**
 * @brief RAII wrapper for AVFrame with copy semantics using av_frame_ref
 * 
 * This class properly manages AVFrame lifetime and provides copy semantics
 * that use FFmpeg's internal reference counting (no deep copy).
 * 
 * Usage:
 *   SharedAVFrame frame;  // Allocates empty frame
 *   avcodec_receive_frame(ctx, frame.get());  // Fill it
 *   
 *   SharedAVFrame copy = frame;  // Uses av_frame_ref (no data copy!)
 *   // Both frame and copy point to same underlying buffers
 */
class SharedAVFrame {
public:
    // ========== Constructors & Destructor ==========
    
    /// Default constructor: allocates empty frame
    SharedAVFrame() : frame_(av_frame_alloc()) {}
    
    /// Destructor: releases frame and its references
    ~SharedAVFrame() {
        if (frame_) {
            av_frame_free(&frame_);
        }
    }
    
    // ========== Copy Operations (uses av_frame_ref) ==========
    
    /// Copy constructor: creates new reference to same data (no deep copy)
    SharedAVFrame(const SharedAVFrame& other) : frame_(av_frame_alloc()) {
        if (other.frame_ && frame_) {
            // av_frame_ref increases reference count, doesn't copy data
            av_frame_ref(frame_, other.frame_);
        }
    }
    
    /// Copy assignment: releases current, references other
    SharedAVFrame& operator=(const SharedAVFrame& other) {
        if (this != &other) {
            // Unref current data
            av_frame_unref(frame_);
            
            // Ref new data
            if (other.frame_) {
                av_frame_ref(frame_, other.frame_);
            }
        }
        return *this;
    }
    
    // ========== Move Operations (transfers ownership) ==========
    
    /// Move constructor: transfers ownership, no refcount change
    SharedAVFrame(SharedAVFrame&& other) noexcept 
        : frame_(other.frame_) 
    {
        other.frame_ = nullptr;
    }
    
    /// Move assignment: releases current, takes ownership
    SharedAVFrame& operator=(SharedAVFrame&& other) noexcept {
        if (this != &other) {
            if (frame_) {
                av_frame_free(&frame_);
            }
            frame_ = other.frame_;
            other.frame_ = nullptr;
        }
        return *this;
    }
    
    // ========== Factory Methods ==========
    
    /// Wrap existing AVFrame by taking reference (not ownership)
    static SharedAVFrame fromRef(const AVFrame* src) {
        SharedAVFrame wrapper;
        if (src && wrapper.frame_) {
            av_frame_ref(wrapper.frame_, src);
        }
        return wrapper;
    }
    
    /// Take ownership of existing AVFrame (caller must not free it)
    static SharedAVFrame fromOwned(AVFrame* src) {
        SharedAVFrame wrapper;
        if (wrapper.frame_) {
            av_frame_free(&wrapper.frame_);
        }
        wrapper.frame_ = src;
        return wrapper;
    }
    
    // ========== Accessors ==========
    
    /// Get raw AVFrame pointer
    [[nodiscard]] AVFrame* get() const { return frame_; }
    
    /// Pointer access operator
    AVFrame* operator->() const { return frame_; }
    
    /// Dereference operator
    AVFrame& operator*() const { return *frame_; }
    
    /// Check if frame is valid (allocated)
    [[nodiscard]] explicit operator bool() const { 
        return frame_ != nullptr; 
    }
    
    /// Check if frame has valid data
    [[nodiscard]] bool hasData() const {
        return frame_ && frame_->data[0] != nullptr;
    }
    
    // ========== Utility Methods ==========
    
    /// Clear frame content (but keep allocation)
    void unref() {
        if (frame_) {
            av_frame_unref(frame_);
        }
    }
    
    /// Create a true deep copy (allocates new buffers)
    [[nodiscard]] SharedAVFrame clone() const {
        SharedAVFrame copy;
        if (frame_ && copy.frame_) {
            av_frame_copy(copy.frame_, frame_);
            av_frame_copy_props(copy.frame_, frame_);
        }
        return copy;
    }
    
    /// Swap with another frame
    void swap(SharedAVFrame& other) noexcept {
        std::swap(frame_, other.frame_);
    }
    
    // ========== Frame Properties ==========
    
    [[nodiscard]] int width() const { 
        return frame_ ? frame_->width : 0; 
    }
    
    [[nodiscard]] int height() const { 
        return frame_ ? frame_->height : 0; 
    }
    
    [[nodiscard]] int format() const { 
        return frame_ ? frame_->format : -1; 
    }
    
    [[nodiscard]] int64_t pts() const { 
        return frame_ ? frame_->pts : AV_NOPTS_VALUE; 
    }
    
    [[nodiscard]] int sampleRate() const {
        return frame_ ? frame_->sample_rate : 0;
    }
    
    [[nodiscard]] int nbSamples() const {
        return frame_ ? frame_->nb_samples : 0;
    }
    
    /// Check if this is a hardware frame
    [[nodiscard]] bool isHardwareFrame() const {
        return frame_ && frame_->hw_frames_ctx != nullptr;
    }
    
private:
    AVFrame* frame_ = nullptr;
};

/// Swap function for ADL
inline void swap(SharedAVFrame& a, SharedAVFrame& b) noexcept {
    a.swap(b);
}

} // namespace phoenix

