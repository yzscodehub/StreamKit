#pragma once

#include <memory>
#include <functional>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <cuda_runtime.h>

namespace StreamKit {

// ============================================================================
// FFmpeg 资源 RAII 包装器
// ============================================================================

/**
 * @brief AVFormatContext 的自定义删除器
 */
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            // 注意：avformat_close_input 会将指针设为 nullptr
            avformat_close_input(&ctx);
        }
    }
};

/**
 * @brief AVCodecContext 的自定义删除器
 */
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};

/**
 * @brief AVFrame 的自定义删除器
 */
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

/**
 * @brief AVPacket 的自定义删除器
 */
struct AVPacketDeleter {
    void operator()(AVPacket* packet) const {
        if (packet) {
            av_packet_free(&packet);
        }
    }
};

/**
 * @brief SwsContext 的自定义删除器
 */
struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const {
        if (ctx) {
            sws_freeContext(ctx);
        }
    }
};

/**
 * @brief SwrContext 的自定义删除器
 */
struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const {
        if (ctx) {
            swr_free(&ctx);
        }
    }
};

/**
 * @brief AVBufferRef (硬件设备上下文) 的自定义删除器
 */
struct AVBufferRefDeleter {
    void operator()(AVBufferRef* ref) const {
        if (ref) {
            av_buffer_unref(&ref);
        }
    }
};

// ============================================================================
// 智能指针类型定义
// ============================================================================

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using AVBufferRefPtr = std::unique_ptr<AVBufferRef, AVBufferRefDeleter>;

// ============================================================================
// 工厂函数 - 创建并管理 FFmpeg 资源
// ============================================================================

/**
 * @brief 创建 AVFormatContext 并自动管理
 * @param url 输入 URL 或文件路径
 * @param fmt 输入格式 (nullptr 表示自动检测)
 * @return 智能指针管理的 AVFormatContext
 */
inline AVFormatContextPtr makeAVFormatContext(const char* url, AVInputFormat* fmt = nullptr) {
    AVFormatContext* ctx = nullptr;
    int ret = avformat_open_input(&ctx, url, fmt, nullptr);
    if (ret < 0) {
        return nullptr;
    }
    return AVFormatContextPtr(ctx);
}

/**
 * @brief 创建 AVCodecContext 并自动管理
 * @param codec 编解码器
 * @return 智能指针管理的 AVCodecContext
 */
inline AVCodecContextPtr makeAVCodecContext(const AVCodec* codec) {
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    return AVCodecContextPtr(ctx);
}

/**
 * @brief 创建 AVFrame 并自动管理
 * @return 智能指针管理的 AVFrame
 */
inline AVFramePtr makeAVFrame() {
    AVFrame* frame = av_frame_alloc();
    return AVFramePtr(frame);
}

/**
 * @brief 创建 AVPacket 并自动管理
 * @return 智能指针管理的 AVPacket
 */
inline AVPacketPtr makeAVPacket() {
    AVPacket* packet = av_packet_alloc();
    return AVPacketPtr(packet);
}

/**
 * @brief 创建 SwsContext 并自动管理
 * @param srcW 源宽度
 * @param srcH 源高度
 * @param srcFormat 源像素格式
 * @param dstW 目标宽度
 * @param dstH 目标高度
 * @param dstFormat 目标像素格式
 * @param flags 缩放算法标志
 * @return 智能指针管理的 SwsContext
 */
inline SwsContextPtr makeSwsContext(
    int srcW, int srcH, AVPixelFormat srcFormat,
    int dstW, int dstH, AVPixelFormat dstFormat,
    int flags = SWS_BILINEAR)
{
    SwsContext* ctx = sws_getContext(
        srcW, srcH, srcFormat,
        dstW, dstH, dstFormat,
        flags, nullptr, nullptr, nullptr
    );
    return SwsContextPtr(ctx);
}

/**
 * @brief 创建 SwrContext 并自动管理
 * @return 智能指针管理的 SwrContext
 */
inline SwrContextPtr makeSwrContext() {
    SwrContext* ctx = swr_alloc();
    return SwrContextPtr(ctx);
}

/**
 * @brief 创建硬件设备缓冲区并自动管理
 * @param type 设备类型 (如 AV_HWDEVICE_TYPE_CUDA)
 * @param device 设备名称 (如 "0" 表示 GPU 0)
 * @param flags 附加标志
 * @return 智能指针管理的 AVBufferRef
 */
inline AVBufferRefPtr makeHWDeviceContext(AVHWDeviceType type, const char* device = nullptr, int flags = 0) {
    AVBufferRef* ref = nullptr;
    int ret = av_hwdevice_ctx_create(&ref, type, device, nullptr, flags);
    if (ret < 0) {
        return nullptr;
    }
    return AVBufferRefPtr(ref);
}

// ============================================================================
// 通用 RAII 作用域守卫
// ============================================================================

/**
 * @brief 通用作用域守卫，用于在作用域退出时执行清理操作
 *
 * 使用示例:
 * @code
 * FILE* file = fopen("data.txt", "r");
 * ScopeGuard guard([&file]() { if (file) fclose(file); });
 * // 使用 file...
 * // 在作用域退出时自动关闭文件
 * @endcode
 */
template<typename Func>
class ScopeGuard {
public:
    explicit ScopeGuard(Func func) : func_(std::move(func)), active_(true) {}

    ~ScopeGuard() {
        if (active_) {
            func_();
        }
    }

    // 禁止拷贝
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    // 支持移动
    ScopeGuard(ScopeGuard&& other) noexcept
        : func_(std::move(other.func_)), active_(other.active_) {
        other.active_ = false;
    }

    /**
     * @brief 取消守卫（不执行清理操作）
     */
    void dismiss() {
        active_ = false;
    }

    /**
     * @brief 立即执行清理操作并取消守卫
     */
    void execute() {
        if (active_) {
            func_();
            active_ = false;
        }
    }

private:
    Func func_;
    bool active_;
};

/**
 * @brief 辅助函数创建 ScopeGuard (类型推导)
 */
template<typename Func>
ScopeGuard<Func> makeScopeGuard(Func func) {
    return ScopeGuard<Func>(std::move(func));
}

// ============================================================================
// CUDA 资源 RAII 包装器
// ============================================================================

/**
 * @brief CUDA 设备指针的删除器
 */
struct CudaDevicePtrDeleter {
    void operator()(void* ptr) const {
        if (ptr) {
            cudaFree(ptr);
        }
    }
};

/**
 * @brief CUDA 托管内存的删除器 (Unified Memory)
 */
struct CudaManagedPtrDeleter {
    void operator()(void* ptr) const {
        if (ptr) {
            cudaFree(ptr);  // cudaFree 适用于所有类型的内存（设备内存和托管内存）
        }
    }
};

/**
 * @brief CUDA 事件删除器
 */
struct CudaEventDeleter {
    void operator()(cudaEvent_t event) const {
        if (event) {
            cudaEventDestroy(event);
        }
    }
};

/**
 * @brief CUDA 流删除器
 */
struct CudaStreamDeleter {
    void operator()(cudaStream_t stream) const {
        if (stream) {
            cudaStreamDestroy(stream);
        }
    }
};

// CUDA 智能指针类型
using CudaDevicePtr = std::unique_ptr<void, CudaDevicePtrDeleter>;
using CudaManagedPtr = std::unique_ptr<void, CudaManagedPtrDeleter>;
using CudaEventPtr = std::unique_ptr<std::remove_pointer<cudaEvent_t>::type, CudaEventDeleter>;
using CudaStreamPtr = std::unique_ptr<std::remove_pointer<cudaStream_t>::type, CudaStreamDeleter>;

/**
 * @brief 创建 CUDA 设备内存并自动管理
 * @param size 内存大小（字节）
 * @return 智能指针管理的设备内存
 */
inline CudaDevicePtr makeCudaDevicePtr(size_t size) {
    void* ptr = nullptr;
    cudaError_t err = cudaMalloc(&ptr, size);
    if (err != cudaSuccess) {
        return nullptr;
    }
    return CudaDevicePtr(ptr);
}

/**
 * @brief 创建 CUDA 托管内存并自动管理
 * @param size 内存大小（字节）
 * @return 智能指针管理的托管内存
 */
inline CudaManagedPtr makeCudaManagedPtr(size_t size) {
    void* ptr = nullptr;
    cudaError_t err = cudaMallocManaged(&ptr, size);
    if (err != cudaSuccess) {
        return nullptr;
    }
    return CudaManagedPtr(ptr);
}

/**
 * @brief 创建 CUDA 事件并自动管理
 * @param flags 事件标志
 * @return 智能指针管理的事件
 */
inline CudaEventPtr makeCudaEvent(unsigned int flags = cudaEventDefault) {
    cudaEvent_t event = nullptr;
    cudaError_t err = cudaEventCreate(&event, flags);
    if (err != cudaSuccess) {
        return nullptr;
    }
    return CudaEventPtr(event);
}

/**
 * @brief 创建 CUDA 流并自动管理
 * @param flags 流标志
 * @return 智能指针管理的流
 */
inline CudaStreamPtr makeCudaStream(unsigned int flags = cudaStreamDefault) {
    cudaStream_t stream = nullptr;
    cudaError_t err = cudaStreamCreateWithFlags(&stream, flags);
    if (err != cudaSuccess) {
        return nullptr;
    }
    return CudaStreamPtr(stream);
}

} // namespace StreamKit
