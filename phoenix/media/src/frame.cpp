/**
 * @file frame.cpp
 * @brief VideoFrame and AudioFrame implementations
 */

#include <phoenix/media/frame.hpp>
#include "ffmpeg/frame_impl.hpp"

namespace phoenix::media {

// ============================================================================
// VideoFrame Implementation
// ============================================================================

VideoFrame::VideoFrame() : m_impl(std::make_shared<Impl>()) {}
VideoFrame::~VideoFrame() = default;

VideoFrame::VideoFrame(const VideoFrame& other) = default;
VideoFrame& VideoFrame::operator=(const VideoFrame& other) = default;
VideoFrame::VideoFrame(VideoFrame&& other) noexcept = default;
VideoFrame& VideoFrame::operator=(VideoFrame&& other) noexcept = default;

int VideoFrame::width() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->width;
}

int VideoFrame::height() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->height;
}

PixelFormat VideoFrame::format() const {
    if (!m_impl || !m_impl->frame) return PixelFormat::Unknown;
    return ff::fromAVPixelFormat(static_cast<AVPixelFormat>(m_impl->frame->format));
}

Timestamp VideoFrame::pts() const {
    if (!m_impl || !m_impl->frame) return kNoTimestamp;
    if (m_impl->frame->pts == AV_NOPTS_VALUE) return kNoTimestamp;
    // Note: pts is already in microseconds when set by decoder
    return m_impl->frame->pts;
}

Duration VideoFrame::duration() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->duration;
}

int64_t VideoFrame::frameNumber() const {
    if (!m_impl) return -1;
    return m_impl->frameNumber;
}

bool VideoFrame::isHardwareFrame() const {
    if (!m_impl || !m_impl->frame) return false;
    return ff::isHardwarePixelFormat(
        static_cast<AVPixelFormat>(m_impl->frame->format));
}

HWAccelType VideoFrame::hwAccelType() const {
    if (!m_impl) return HWAccelType::None;
    return m_impl->hwType;
}

Result<VideoFrame, Error> VideoFrame::transferToCPU() const {
    if (!isValid()) {
        return Error(ErrorCode::InvalidArgument, "Invalid frame");
    }
    
    if (!isHardwareFrame()) {
        // Already CPU frame, return a reference copy
        VideoFrame copy;
        copy.m_impl = std::make_shared<Impl>();
        copy.m_impl->frame = m_impl->frame.ref();
        copy.m_impl->frameNumber = m_impl->frameNumber;
        return copy;
    }
    
    // Transfer from GPU to CPU
    auto swFrame = ff::SharedAVFrame::alloc();
    if (!swFrame) {
        return Error(ErrorCode::OutOfMemory, "Failed to allocate frame");
    }
    
    int ret = av_hwframe_transfer_data(swFrame.get(), m_impl->frame.get(), 0);
    if (ret < 0) {
        return ff::avError(ret, "Failed to transfer hardware frame");
    }
    
    // Copy properties
    swFrame->pts = m_impl->frame->pts;
    swFrame->duration = m_impl->frame->duration;
    
    VideoFrame result;
    result.m_impl = std::make_shared<Impl>(std::move(swFrame), HWAccelType::None);
    result.m_impl->frameNumber = m_impl->frameNumber;
    return result;
}

int VideoFrame::planeCount() const {
    if (!m_impl || !m_impl->frame) return 0;
    
    // Count non-null data pointers
    int count = 0;
    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        if (m_impl->frame->data[i]) ++count;
        else break;
    }
    return count;
}

const uint8_t* VideoFrame::data(int plane) const {
    if (!m_impl || !m_impl->frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return nullptr;
    }
    if (isHardwareFrame()) return nullptr;  // Must transfer first
    return m_impl->frame->data[plane];
}

uint8_t* VideoFrame::data(int plane) {
    if (!m_impl || !m_impl->frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return nullptr;
    }
    if (isHardwareFrame()) return nullptr;
    return m_impl->frame->data[plane];
}

int VideoFrame::linesize(int plane) const {
    if (!m_impl || !m_impl->frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return 0;
    }
    return m_impl->frame->linesize[plane];
}

bool VideoFrame::isValid() const {
    return m_impl && m_impl->frame && m_impl->frame->width > 0;
}

Result<VideoFrame, Error> VideoFrame::create(int width, int height, PixelFormat format) {
    if (width <= 0 || height <= 0) {
        return Error(ErrorCode::InvalidArgument, "Invalid dimensions");
    }
    
    auto frame = ff::SharedAVFrame::alloc();
    if (!frame) {
        return Error(ErrorCode::OutOfMemory, "Failed to allocate frame");
    }
    
    frame->width = width;
    frame->height = height;
    frame->format = ff::toAVPixelFormat(format);
    
    int ret = av_frame_get_buffer(frame.get(), 0);
    if (ret < 0) {
        return ff::avError(ret, "Failed to allocate frame buffer");
    }
    
    VideoFrame result;
    result.m_impl = std::make_shared<Impl>(std::move(frame));
    return result;
}

// ============================================================================
// AudioFrame Implementation
// ============================================================================

AudioFrame::AudioFrame() : m_impl(std::make_shared<Impl>()) {}
AudioFrame::~AudioFrame() = default;

AudioFrame::AudioFrame(const AudioFrame& other) = default;
AudioFrame& AudioFrame::operator=(const AudioFrame& other) = default;
AudioFrame::AudioFrame(AudioFrame&& other) noexcept = default;
AudioFrame& AudioFrame::operator=(AudioFrame&& other) noexcept = default;

int AudioFrame::sampleRate() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->sample_rate;
}

int AudioFrame::channels() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->ch_layout.nb_channels;
}

int AudioFrame::sampleCount() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->nb_samples;
}

SampleFormat AudioFrame::format() const {
    if (!m_impl || !m_impl->frame) return SampleFormat::Unknown;
    return ff::fromAVSampleFormat(
        static_cast<AVSampleFormat>(m_impl->frame->format));
}

Timestamp AudioFrame::pts() const {
    if (!m_impl || !m_impl->frame) return kNoTimestamp;
    if (m_impl->frame->pts == AV_NOPTS_VALUE) return kNoTimestamp;
    return m_impl->frame->pts;
}

Duration AudioFrame::duration() const {
    if (!m_impl || !m_impl->frame) return 0;
    return m_impl->frame->duration;
}

bool AudioFrame::isPlanar() const {
    if (!m_impl || !m_impl->frame) return false;
    return av_sample_fmt_is_planar(
        static_cast<AVSampleFormat>(m_impl->frame->format));
}

const uint8_t* AudioFrame::data(int plane) const {
    if (!m_impl || !m_impl->frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return nullptr;
    }
    return m_impl->frame->data[plane];
}

uint8_t* AudioFrame::data(int plane) {
    if (!m_impl || !m_impl->frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return nullptr;
    }
    return m_impl->frame->data[plane];
}

int AudioFrame::linesize(int plane) const {
    if (!m_impl || !m_impl->frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
        return 0;
    }
    return m_impl->frame->linesize[plane];
}

size_t AudioFrame::dataSize() const {
    if (!m_impl || !m_impl->frame) return 0;
    
    int bytesPerSample = av_get_bytes_per_sample(
        static_cast<AVSampleFormat>(m_impl->frame->format));
    
    if (isPlanar()) {
        return static_cast<size_t>(bytesPerSample) * m_impl->frame->nb_samples;
    } else {
        return static_cast<size_t>(bytesPerSample) * 
               m_impl->frame->nb_samples * channels();
    }
}

bool AudioFrame::isValid() const {
    return m_impl && m_impl->frame && m_impl->frame->nb_samples > 0;
}

Result<AudioFrame, Error> AudioFrame::create(
    int sampleCount, int channels, int sampleRate, SampleFormat format)
{
    if (sampleCount <= 0 || channels <= 0 || sampleRate <= 0) {
        return Error(ErrorCode::InvalidArgument, "Invalid audio parameters");
    }
    
    auto frame = ff::SharedAVFrame::alloc();
    if (!frame) {
        return Error(ErrorCode::OutOfMemory, "Failed to allocate frame");
    }
    
    frame->nb_samples = sampleCount;
    frame->sample_rate = sampleRate;
    frame->format = ff::toAVSampleFormat(format);
    
    // Set channel layout
    AVChannelLayout layout{};
    av_channel_layout_default(&layout, channels);
    av_channel_layout_copy(&frame->ch_layout, &layout);
    av_channel_layout_uninit(&layout);
    
    int ret = av_frame_get_buffer(frame.get(), 0);
    if (ret < 0) {
        return ff::avError(ret, "Failed to allocate audio buffer");
    }
    
    AudioFrame result;
    result.m_impl = std::make_shared<Impl>(std::move(frame));
    return result;
}

} // namespace phoenix::media
