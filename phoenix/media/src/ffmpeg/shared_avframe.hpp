/**
 * @file shared_avframe.hpp
 * @brief RAII wrapper for AVFrame with reference counting
 * 
 * SharedAVFrame provides automatic memory management for AVFrame,
 * supporting both owned and reference-counted frames.
 */

#pragma once

#include "ff_common.hpp"
#include <memory>

namespace phoenix::media::ff {

/**
 * @brief Deleter for AVFrame
 */
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

/**
 * @brief RAII wrapper for AVFrame
 * 
 * Uses shared_ptr for reference-counted ownership.
 * Supports both allocated frames and referenced frames.
 */
class SharedAVFrame {
public:
    /// Create empty frame
    SharedAVFrame() = default;
    
    /// Create and allocate a new frame
    static SharedAVFrame alloc() {
        SharedAVFrame frame;
        frame.m_frame.reset(av_frame_alloc(), AVFrameDeleter{});
        return frame;
    }
    
    /// Wrap an existing AVFrame (takes ownership)
    static SharedAVFrame wrap(AVFrame* frame) {
        SharedAVFrame f;
        f.m_frame.reset(frame, AVFrameDeleter{});
        return f;
    }
    
    /// Create a reference to another frame's data
    SharedAVFrame ref() const {
        if (!m_frame) return {};
        
        SharedAVFrame copy;
        copy.m_frame.reset(av_frame_alloc(), AVFrameDeleter{});
        if (av_frame_ref(copy.m_frame.get(), m_frame.get()) < 0) {
            return {};
        }
        return copy;
    }
    
    /// Create an independent copy
    SharedAVFrame clone() const {
        if (!m_frame) return {};
        
        SharedAVFrame copy;
        copy.m_frame.reset(av_frame_clone(m_frame.get()), AVFrameDeleter{});
        return copy;
    }
    
    /// Get raw pointer
    AVFrame* get() const { return m_frame.get(); }
    
    /// Arrow operator
    AVFrame* operator->() const { return m_frame.get(); }
    
    /// Dereference
    AVFrame& operator*() const { return *m_frame; }
    
    /// Check if valid
    explicit operator bool() const { return m_frame != nullptr; }
    
    /// Reset the frame
    void reset() { m_frame.reset(); }
    
    /// Unreference the frame data (keep allocation)
    void unref() {
        if (m_frame) {
            av_frame_unref(m_frame.get());
        }
    }
    
    /// Get use count
    long useCount() const { return m_frame.use_count(); }
    
private:
    std::shared_ptr<AVFrame> m_frame;
};

/**
 * @brief Deleter for AVPacket
 */
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
};

/**
 * @brief RAII wrapper for AVPacket
 */
class SharedAVPacket {
public:
    SharedAVPacket() = default;
    
    static SharedAVPacket alloc() {
        SharedAVPacket pkt;
        pkt.m_packet.reset(av_packet_alloc(), AVPacketDeleter{});
        return pkt;
    }
    
    static SharedAVPacket wrap(AVPacket* packet) {
        SharedAVPacket p;
        p.m_packet.reset(packet, AVPacketDeleter{});
        return p;
    }
    
    SharedAVPacket ref() const {
        if (!m_packet) return {};
        
        SharedAVPacket copy;
        copy.m_packet.reset(av_packet_alloc(), AVPacketDeleter{});
        if (av_packet_ref(copy.m_packet.get(), m_packet.get()) < 0) {
            return {};
        }
        return copy;
    }
    
    AVPacket* get() const { return m_packet.get(); }
    AVPacket* operator->() const { return m_packet.get(); }
    AVPacket& operator*() const { return *m_packet; }
    explicit operator bool() const { return m_packet != nullptr; }
    
    void reset() { m_packet.reset(); }
    void unref() {
        if (m_packet) {
            av_packet_unref(m_packet.get());
        }
    }
    
private:
    std::shared_ptr<AVPacket> m_packet;
};

} // namespace phoenix::media::ff
