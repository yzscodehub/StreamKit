/**
 * @file renderer.hpp
 * @brief Renderer interfaces and frame uploader abstraction
 * 
 * Provides abstractions for:
 * - IFrameUploader: Upload frames to GPU (software or hardware path)
 * - IRenderer: Display frames and handle window
 */

#pragma once

#include <memory>

#include "core/types.hpp"
#include "core/result.hpp"
#include "core/media_frame.hpp"

namespace phoenix {

// ============================================================================
// IFrameUploader - Uploads frames to render surface
// ============================================================================

/**
 * @brief Interface for uploading frames to GPU
 * 
 * Implementations:
 * - SoftwareUploader: Uploads YUV data to texture
 * - D3D11Uploader: Zero-copy from hardware decoder (Phase 4)
 */
class IFrameUploader {
public:
    virtual ~IFrameUploader() = default;
    
    /**
     * @brief Upload a frame and return texture handle
     * 
     * @param frame VideoFrame to upload
     * @return Native texture handle (SDL_Texture*, ID3D11Texture2D*, etc.)
     */
    virtual void* upload(const VideoFrame& frame) = 0;
    
    /**
     * @brief Check if uploader supports a pixel format
     */
    virtual bool supportsFormat(PixelFormat format) const = 0;
    
    /**
     * @brief Get the output format (format after upload)
     */
    virtual PixelFormat outputFormat() const = 0;
};

// ============================================================================
// IRenderer - Window and rendering interface
// ============================================================================

/**
 * @brief Interface for window management and rendering
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // ========== Lifecycle ==========
    
    /**
     * @brief Initialize the renderer
     * @param width Initial window width
     * @param height Initial window height
     * @param title Window title
     * @return Ok on success
     */
    virtual Result<void> init(int width, int height, const char* title) = 0;
    
    /**
     * @brief Shutdown the renderer
     */
    virtual void shutdown() = 0;
    
    // ========== Rendering ==========
    
    /**
     * @brief Draw a video frame
     * @param frame Frame to draw
     * @return Ok on success
     */
    virtual Result<void> draw(const VideoFrame& frame) = 0;
    
    /**
     * @brief Present the current frame
     */
    virtual void present() = 0;
    
    /**
     * @brief Clear the screen
     */
    virtual void clear() = 0;
    
    // ========== Window Management ==========
    
    /**
     * @brief Resize the render surface
     */
    virtual void resize(int width, int height) = 0;
    
    /**
     * @brief Get current window size
     */
    virtual void getSize(int* width, int* height) const = 0;
    
    /**
     * @brief Check if window is still open
     */
    virtual bool isOpen() const = 0;
    
    /**
     * @brief Get the frame uploader
     */
    virtual IFrameUploader* uploader() = 0;
};

// ============================================================================
// Renderer Factory
// ============================================================================

using RendererPtr = std::unique_ptr<IRenderer>;

/**
 * @brief Create a renderer
 * 
 * Currently only SDL2 is supported.
 */
RendererPtr createRenderer();

} // namespace phoenix

