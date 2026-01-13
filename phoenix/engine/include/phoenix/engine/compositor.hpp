/**
 * @file compositor.hpp
 * @brief Video compositor for multi-track compositing
 * 
 * Composites video frames from multiple tracks into
 * a single output frame.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/media/frame.hpp>
#include <phoenix/model/sequence.hpp>
#include <phoenix/engine/frame_cache.hpp>

#include <memory>
#include <vector>
#include <functional>

namespace phoenix::engine {

/**
 * @brief Blend mode for compositing
 */
enum class BlendMode {
    Normal,      ///< Normal alpha compositing
    Add,         ///< Additive blending
    Multiply,    ///< Multiply blending
    Screen,      ///< Screen blending
    Overlay,     ///< Overlay blending
    Difference,  ///< Difference blending
};

/**
 * @brief Layer for compositing
 */
struct CompositeLayer {
    std::shared_ptr<media::VideoFrame> frame;
    BlendMode blendMode = BlendMode::Normal;
    float opacity = 1.0f;
    
    // Transform (for future use)
    float x = 0.0f;
    float y = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
};

/**
 * @brief Result of composition
 */
struct CompositeResult {
    std::shared_ptr<media::VideoFrame> frame;
    Timestamp timestamp;
    bool hasVideo = false;
    bool hasAudio = false;
};

/**
 * @brief Frame request for decoding
 */
struct FrameRequest {
    UUID clipId;
    UUID mediaItemId;
    Timestamp mediaTime;
    int trackIndex;
};

/**
 * @brief Callback type for frame decoding
 */
using FrameDecoderCallback = std::function<
    std::shared_ptr<media::VideoFrame>(const FrameRequest&)
>;

/**
 * @brief Video compositor
 * 
 * Composites video frames from multiple tracks based on
 * a sequence's timeline. Handles track stacking order,
 * blending, and basic transforms.
 * 
 * Usage:
 * @code
 *   Compositor compositor(1920, 1080);
 *   compositor.setSequence(&sequence);
 *   compositor.setFrameDecoder([&](const FrameRequest& req) {
 *       return decoder.decodeFrame(req.mediaItemId, req.mediaTime);
 *   });
 *   
 *   auto result = compositor.compose(currentTime);
 * @endcode
 */
class Compositor {
public:
    /**
     * @brief Construct compositor with output dimensions
     */
    Compositor(int width, int height)
        : m_outputWidth(width)
        , m_outputHeight(height) {}
    
    // ========== Configuration ==========
    
    /**
     * @brief Set the sequence to composite
     */
    void setSequence(const model::Sequence* sequence) {
        m_sequence = sequence;
    }
    
    /**
     * @brief Set frame decoder callback
     */
    void setFrameDecoder(FrameDecoderCallback decoder) {
        m_decoder = std::move(decoder);
    }
    
    /**
     * @brief Set output dimensions
     */
    void setOutputSize(int width, int height) {
        m_outputWidth = width;
        m_outputHeight = height;
    }
    
    /**
     * @brief Set background color (RGBA)
     */
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        m_bgColor = {r, g, b, a};
    }
    
    // ========== Composition ==========
    
    /**
     * @brief Compose frame at given timeline position
     * 
     * @param time Timeline position
     * @return Composited frame result
     */
    CompositeResult compose(Timestamp time) {
        CompositeResult result;
        result.timestamp = time;
        
        if (!m_sequence) {
            result.frame = createBlankFrame();
            return result;
        }
        
        // Collect layers from all tracks
        std::vector<CompositeLayer> layers;
        
        // Get clips at current time (bottom to top order)
        const auto& tracks = m_sequence->videoTracks();
        for (size_t i = 0; i < tracks.size(); ++i) {
            const auto& track = tracks[i];
            
            // Skip hidden/muted tracks
            if (track->hidden() || track->muted()) continue;
            
            // Find clip at current time
            auto clip = track->getClipAt(time);
            if (!clip) continue;
            
            // Skip disabled clips
            if (clip->disabled()) continue;
            
            // Calculate source time using clip's mapToSource
            Timestamp sourceTime = clip->mapToSource(time);
            
            // Request frame from decoder
            if (m_decoder) {
                FrameRequest request{
                    clip->id(),
                    clip->mediaItemId(),
                    sourceTime,
                    static_cast<int>(i)
                };
                
                auto frame = m_decoder(request);
                if (frame) {
                    CompositeLayer layer;
                    layer.frame = frame;
                    layer.opacity = clip->opacity();
                    // TODO: Get blend mode and transform from clip
                    
                    layers.push_back(std::move(layer));
                    result.hasVideo = true;
                }
            }
        }
        
        // Composite layers
        if (layers.empty()) {
            result.frame = createBlankFrame();
        } else if (layers.size() == 1 && 
                   layers[0].opacity >= 1.0f &&
                   layers[0].blendMode == BlendMode::Normal) {
            // Single layer, no processing needed
            result.frame = layers[0].frame;
        } else {
            result.frame = compositeLayers(layers);
        }
        
        return result;
    }
    
    /**
     * @brief Get list of clips visible at given time
     */
    std::vector<FrameRequest> getVisibleClips(Timestamp time) const {
        std::vector<FrameRequest> requests;
        
        if (!m_sequence) return requests;
        
        const auto& tracks = m_sequence->videoTracks();
        for (size_t i = 0; i < tracks.size(); ++i) {
            const auto& track = tracks[i];
            if (track->hidden() || track->muted()) continue;
            
            auto clip = track->getClipAt(time);
            if (!clip || clip->disabled()) continue;
            
            Timestamp sourceTime = clip->mapToSource(time);
            
            requests.push_back({
                clip->id(),
                clip->mediaItemId(),
                sourceTime,
                static_cast<int>(i)
            });
        }
        
        return requests;
    }
    
    // ========== Accessors ==========
    
    [[nodiscard]] int outputWidth() const { return m_outputWidth; }
    [[nodiscard]] int outputHeight() const { return m_outputHeight; }
    
private:
    /**
     * @brief Create blank (background color) frame
     */
    std::shared_ptr<media::VideoFrame> createBlankFrame() {
        auto result = media::VideoFrame::create(m_outputWidth, m_outputHeight, PixelFormat::RGBA);
        if (!result) {
            return nullptr;
        }
        
        auto frame = std::make_shared<media::VideoFrame>(std::move(result.value()));
        
        // Fill with background color
        uint8_t* data = frame->data(0);
        if (data) {
            size_t size = static_cast<size_t>(m_outputWidth) * m_outputHeight * 4;
            for (size_t i = 0; i < size; i += 4) {
                data[i + 0] = m_bgColor[0];
                data[i + 1] = m_bgColor[1];
                data[i + 2] = m_bgColor[2];
                data[i + 3] = m_bgColor[3];
            }
        }
        
        return frame;
    }
    
    /**
     * @brief Composite multiple layers into single frame
     */
    std::shared_ptr<media::VideoFrame> compositeLayers(
            const std::vector<CompositeLayer>& layers) {
        // Start with blank canvas
        auto result = createBlankFrame();
        
        // Composite each layer (bottom to top)
        for (const auto& layer : layers) {
            if (!layer.frame) continue;
            
            blendLayer(*result, *layer.frame, layer.blendMode, layer.opacity);
        }
        
        return result;
    }
    
    /**
     * @brief Blend source layer onto destination
     */
    void blendLayer(media::VideoFrame& dst,
                    const media::VideoFrame& src,
                    BlendMode mode,
                    float opacity) {
        // Simple alpha blending for now
        // TODO: Implement other blend modes
        // TODO: Handle different formats
        // TODO: Implement scaling/transform
        
        if (src.format() != PixelFormat::RGBA || dst.format() != PixelFormat::RGBA) {
            // Need format conversion
            return;
        }
        
        int minWidth = std::min(dst.width(), src.width());
        int minHeight = std::min(dst.height(), src.height());
        
        const uint8_t* srcData = src.data(0);
        uint8_t* dstData = dst.data(0);
        
        if (!srcData || !dstData) return;
        
        int srcLinesize = src.linesize(0);
        int dstLinesize = dst.linesize(0);
        
        for (int y = 0; y < minHeight; ++y) {
            for (int x = 0; x < minWidth; ++x) {
                size_t srcIdx = static_cast<size_t>(y * srcLinesize + x * 4);
                size_t dstIdx = static_cast<size_t>(y * dstLinesize + x * 4);
                
                float srcR = srcData[srcIdx + 0] / 255.0f;
                float srcG = srcData[srcIdx + 1] / 255.0f;
                float srcB = srcData[srcIdx + 2] / 255.0f;
                float srcA = (srcData[srcIdx + 3] / 255.0f) * opacity;
                
                float dstR = dstData[dstIdx + 0] / 255.0f;
                float dstG = dstData[dstIdx + 1] / 255.0f;
                float dstB = dstData[dstIdx + 2] / 255.0f;
                float dstA = dstData[dstIdx + 3] / 255.0f;
                
                // Apply blend mode
                float outR, outG, outB, outA;
                blendPixel(mode, srcR, srcG, srcB, srcA,
                          dstR, dstG, dstB, dstA,
                          outR, outG, outB, outA);
                
                dstData[dstIdx + 0] = static_cast<uint8_t>(outR * 255.0f);
                dstData[dstIdx + 1] = static_cast<uint8_t>(outG * 255.0f);
                dstData[dstIdx + 2] = static_cast<uint8_t>(outB * 255.0f);
                dstData[dstIdx + 3] = static_cast<uint8_t>(outA * 255.0f);
            }
        }
    }
    
    /**
     * @brief Blend single pixel
     */
    static void blendPixel(BlendMode mode,
                          float srcR, float srcG, float srcB, float srcA,
                          float dstR, float dstG, float dstB, float dstA,
                          float& outR, float& outG, float& outB, float& outA) {
        switch (mode) {
            case BlendMode::Normal: {
                // Standard alpha compositing (Porter-Duff over)
                outA = srcA + dstA * (1.0f - srcA);
                if (outA > 0.0f) {
                    outR = (srcR * srcA + dstR * dstA * (1.0f - srcA)) / outA;
                    outG = (srcG * srcA + dstG * dstA * (1.0f - srcA)) / outA;
                    outB = (srcB * srcA + dstB * dstA * (1.0f - srcA)) / outA;
                } else {
                    outR = outG = outB = 0.0f;
                }
                break;
            }
            
            case BlendMode::Add: {
                outR = std::min(1.0f, srcR * srcA + dstR);
                outG = std::min(1.0f, srcG * srcA + dstG);
                outB = std::min(1.0f, srcB * srcA + dstB);
                outA = std::min(1.0f, srcA + dstA);
                break;
            }
            
            case BlendMode::Multiply: {
                float blendR = srcR * dstR;
                float blendG = srcG * dstG;
                float blendB = srcB * dstB;
                outR = blendR * srcA + dstR * (1.0f - srcA);
                outG = blendG * srcA + dstG * (1.0f - srcA);
                outB = blendB * srcA + dstB * (1.0f - srcA);
                outA = srcA + dstA * (1.0f - srcA);
                break;
            }
            
            case BlendMode::Screen: {
                float blendR = 1.0f - (1.0f - srcR) * (1.0f - dstR);
                float blendG = 1.0f - (1.0f - srcG) * (1.0f - dstG);
                float blendB = 1.0f - (1.0f - srcB) * (1.0f - dstB);
                outR = blendR * srcA + dstR * (1.0f - srcA);
                outG = blendG * srcA + dstG * (1.0f - srcA);
                outB = blendB * srcA + dstB * (1.0f - srcA);
                outA = srcA + dstA * (1.0f - srcA);
                break;
            }
            
            case BlendMode::Overlay: {
                auto overlay = [](float a, float b) {
                    return a < 0.5f 
                        ? 2.0f * a * b 
                        : 1.0f - 2.0f * (1.0f - a) * (1.0f - b);
                };
                float blendR = overlay(dstR, srcR);
                float blendG = overlay(dstG, srcG);
                float blendB = overlay(dstB, srcB);
                outR = blendR * srcA + dstR * (1.0f - srcA);
                outG = blendG * srcA + dstG * (1.0f - srcA);
                outB = blendB * srcA + dstB * (1.0f - srcA);
                outA = srcA + dstA * (1.0f - srcA);
                break;
            }
            
            case BlendMode::Difference: {
                float blendR = std::abs(srcR - dstR);
                float blendG = std::abs(srcG - dstG);
                float blendB = std::abs(srcB - dstB);
                outR = blendR * srcA + dstR * (1.0f - srcA);
                outG = blendG * srcA + dstG * (1.0f - srcA);
                outB = blendB * srcA + dstB * (1.0f - srcA);
                outA = srcA + dstA * (1.0f - srcA);
                break;
            }
        }
    }
    
private:
    const model::Sequence* m_sequence = nullptr;
    FrameDecoderCallback m_decoder;
    
    int m_outputWidth;
    int m_outputHeight;
    
    std::array<uint8_t, 4> m_bgColor = {0, 0, 0, 255};  // Black
};

} // namespace phoenix::engine
