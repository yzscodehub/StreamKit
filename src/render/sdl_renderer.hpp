/**
 * @file sdl_renderer.hpp
 * @brief SDL2 implementation of IRenderer
 * 
 * Provides:
 * - SDLSoftwareUploader: Uploads YUV frames to SDL_Texture
 * - SDLRenderer: SDL2 window and rendering
 */

#pragma once

#include <memory>
#include <mutex>

#include <SDL2/SDL.h>

#include "renderer.hpp"
#include "core/types.hpp"
#include "core/media_frame.hpp"

#include <spdlog/spdlog.h>

namespace phoenix {

// ============================================================================
// SDLSoftwareUploader
// ============================================================================

/**
 * @brief Uploads software frames (YUV) to SDL texture
 */
class SDLSoftwareUploader : public IFrameUploader {
public:
    SDLSoftwareUploader(SDL_Renderer* renderer)
        : renderer_(renderer)
    {}
    
    ~SDLSoftwareUploader() override {
        if (texture_) {
            SDL_DestroyTexture(texture_);
        }
    }
    
    void* upload(const VideoFrame& frame) override {
        if (!frame.isSoftware()) {
            spdlog::error("SDLSoftwareUploader: Frame is not software frame");
            return nullptr;
        }
        
        const auto& sw = frame.asSoftware();
        AVFrame* avf = sw.avFrame.get();
        
        if (!avf || !avf->data[0]) {
            return nullptr;
        }
        
        // Ensure texture exists and matches frame size
        if (!ensureTexture(frame.width, frame.height, frame.format)) {
            return nullptr;
        }
        
        // Upload YUV data
        int ret = 0;
        
        switch (frame.format) {
            case PixelFormat::YUV420P:
            case PixelFormat::YUV422P:
            case PixelFormat::YUV444P:
                ret = SDL_UpdateYUVTexture(
                    texture_, nullptr,
                    avf->data[0], avf->linesize[0],  // Y
                    avf->data[1], avf->linesize[1],  // U
                    avf->data[2], avf->linesize[2]   // V
                );
                break;
                
            case PixelFormat::NV12:
                ret = SDL_UpdateNVTexture(
                    texture_, nullptr,
                    avf->data[0], avf->linesize[0],  // Y
                    avf->data[1], avf->linesize[1]   // UV
                );
                break;
                
            default:
                spdlog::error("Unsupported pixel format: {}", static_cast<int>(frame.format));
                return nullptr;
        }
        
        if (ret != 0) {
            spdlog::error("SDL texture update failed: {}", SDL_GetError());
            return nullptr;
        }
        
        return texture_;
    }
    
    bool supportsFormat(PixelFormat format) const override {
        switch (format) {
            case PixelFormat::YUV420P:
            case PixelFormat::YUV422P:
            case PixelFormat::YUV444P:
            case PixelFormat::NV12:
                return true;
            default:
                return false;
        }
    }
    
    PixelFormat outputFormat() const override {
        return PixelFormat::YUV420P;  // SDL converts to display format
    }
    
private:
    bool ensureTexture(int width, int height, PixelFormat format) {
        if (texture_ && textureWidth_ == width && textureHeight_ == height) {
            return true;  // Reuse existing
        }
        
        // Destroy old texture
        if (texture_) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        
        // Determine SDL format
        Uint32 sdlFormat = SDL_PIXELFORMAT_IYUV;  // Default YUV420P
        
        switch (format) {
            case PixelFormat::YUV420P:
                sdlFormat = SDL_PIXELFORMAT_IYUV;
                break;
            case PixelFormat::YUV422P:
                sdlFormat = SDL_PIXELFORMAT_YUY2;
                break;
            case PixelFormat::NV12:
                sdlFormat = SDL_PIXELFORMAT_NV12;
                break;
            default:
                sdlFormat = SDL_PIXELFORMAT_IYUV;
                break;
        }
        
        // Create texture
        texture_ = SDL_CreateTexture(
            renderer_,
            sdlFormat,
            SDL_TEXTUREACCESS_STREAMING,
            width, height
        );
        
        if (!texture_) {
            spdlog::error("Failed to create texture: {}", SDL_GetError());
            return false;
        }
        
        textureWidth_ = width;
        textureHeight_ = height;
        
        spdlog::debug("Created texture {}x{}", width, height);
        return true;
    }
    
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
};

// ============================================================================
// SDLRenderer
// ============================================================================

/**
 * @brief SDL2 implementation of IRenderer
 */
class SDLRenderer : public IRenderer {
public:
    SDLRenderer() = default;
    
    ~SDLRenderer() override {
        shutdown();
    }
    
    // ========== Lifecycle ==========
    
    Result<void> init(int width, int height, const char* title) override {
        std::lock_guard lock(mutex_);
        
        if (window_) {
            return Err(ErrorCode::InvalidArgument, "Already initialized");
        }
        
        // Create window
        window_ = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
        );
        
        if (!window_) {
            return Err(ErrorCode::WindowCreationFailed, SDL_GetError());
        }
        
        // Create renderer
        renderer_ = SDL_CreateRenderer(
            window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
        
        if (!renderer_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            return Err(ErrorCode::RenderError, SDL_GetError());
        }
        
        // Create uploader
        uploader_ = std::make_unique<SDLSoftwareUploader>(renderer_);
        
        windowWidth_ = width;
        windowHeight_ = height;
        
        spdlog::info("SDLRenderer initialized: {}x{}", width, height);
        return Ok();
    }
    
    void shutdown() override {
        std::lock_guard lock(mutex_);
        
        uploader_.reset();
        
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
    }
    
    // ========== Rendering ==========
    
    Result<void> draw(const VideoFrame& frame) override {
        std::lock_guard lock(mutex_);
        
        if (!renderer_ || !uploader_) {
            return Err(ErrorCode::RenderError, "Renderer not initialized");
        }
        
        // Upload frame to texture
        void* texture = uploader_->upload(frame);
        if (!texture) {
            return Err(ErrorCode::TextureCreationFailed, "Failed to upload frame");
        }
        
        // Calculate display rect (maintain aspect ratio)
        SDL_Rect dstRect = calculateDisplayRect(frame.width, frame.height);
        
        // Clear and draw
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, static_cast<SDL_Texture*>(texture), nullptr, &dstRect);
        
        return Ok();
    }
    
    void present() override {
        std::lock_guard lock(mutex_);
        
        if (renderer_) {
            SDL_RenderPresent(renderer_);
        }
    }
    
    void clear() override {
        std::lock_guard lock(mutex_);
        
        if (renderer_) {
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
            SDL_RenderClear(renderer_);
        }
    }
    
    // ========== Window Management ==========
    
    void resize(int width, int height) override {
        std::lock_guard lock(mutex_);
        
        windowWidth_ = width;
        windowHeight_ = height;
    }
    
    void getSize(int* width, int* height) const override {
        std::lock_guard lock(mutex_);
        
        if (width) *width = windowWidth_;
        if (height) *height = windowHeight_;
    }
    
    bool isOpen() const override {
        std::lock_guard lock(mutex_);
        return window_ != nullptr;
    }
    
    IFrameUploader* uploader() override {
        return uploader_.get();
    }
    
    // ========== SDL-specific ==========
    
    SDL_Window* window() { return window_; }
    SDL_Renderer* renderer() { return renderer_; }
    
private:
    SDL_Rect calculateDisplayRect(int frameWidth, int frameHeight) {
        float frameAspect = static_cast<float>(frameWidth) / frameHeight;
        float windowAspect = static_cast<float>(windowWidth_) / windowHeight_;
        
        SDL_Rect rect;
        
        if (frameAspect > windowAspect) {
            // Frame is wider - fit to width
            rect.w = windowWidth_;
            rect.h = static_cast<int>(windowWidth_ / frameAspect);
            rect.x = 0;
            rect.y = (windowHeight_ - rect.h) / 2;
        } else {
            // Frame is taller - fit to height
            rect.h = windowHeight_;
            rect.w = static_cast<int>(windowHeight_ * frameAspect);
            rect.x = (windowWidth_ - rect.w) / 2;
            rect.y = 0;
        }
        
        return rect;
    }
    
    mutable std::mutex mutex_;
    
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<SDLSoftwareUploader> uploader_;
    
    int windowWidth_ = 0;
    int windowHeight_ = 0;
};

// ============================================================================
// Factory Implementation
// ============================================================================

inline RendererPtr createRenderer() {
    return std::make_unique<SDLRenderer>();
}

} // namespace phoenix

