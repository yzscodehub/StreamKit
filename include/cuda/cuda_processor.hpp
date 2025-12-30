#pragma once

#include <cuda_runtime.h>
#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <libavutil/frame.h>

namespace StreamKit {

struct GPUMemorySync {
    void* gpu_buffer;
    cudaEvent_t ready_event;
    bool is_ready;
    size_t size;
    
    GPUMemorySync() : gpu_buffer(nullptr), is_ready(false), size(0) {
        cudaEventCreate(&ready_event);
    }
    
    ~GPUMemorySync() {
        if (gpu_buffer) {
            cudaFree(gpu_buffer);
        }
        cudaEventDestroy(ready_event);
    }
};

class CudaProcessor {
public:
    CudaProcessor();
    ~CudaProcessor();
    
    // 初始化CUDA
    bool initialize(int device_id = 0);
    
    // 视频帧GPU处理
    bool processVideoFrame(AVFrame* input_frame, AVFrame* output_frame);
    
    // 音频数据GPU处理
    bool processAudioData(const uint8_t* input, uint8_t* output, int samples);
    
    // 异步处理
    void asyncProcessVideoFrame(AVFrame* input_frame, AVFrame* output_frame);
    void asyncProcessAudioData(const uint8_t* input, uint8_t* output, int samples);
    
    // 等待处理完成
    void waitForVideoProcessing();
    void waitForAudioProcessing();
    
    // 同步GPU操作
    void synchronize();
    
    // 获取GPU内存
    void* allocateGPUMemory(size_t size);
    void freeGPUMemory(void* ptr);
    
    // 内存拷贝
    bool copyToGPU(const void* host_ptr, void* device_ptr, size_t size);
    bool copyFromGPU(const void* device_ptr, void* host_ptr, size_t size);
    
    // 检查GPU状态
    bool isGPUReady() const;
    int getDeviceId() const { return device_id_; }
    
    // 清理资源
    void cleanup();

private:
    // CUDA流和事件
    cudaStream_t video_stream_;
    cudaStream_t audio_stream_;
    cudaEvent_t video_event_;
    cudaEvent_t audio_event_;
    
    // GPU内存管理
    std::map<void*, GPUMemorySync> gpu_memory_sync_;
    
    // 设备信息
    int device_id_;
    cudaDeviceProp device_prop_;
    
    // 状态标志
    bool initialized_;
    
    // 线程安全
    mutable std::mutex mutex_;
    
    // 内存池
    struct MemoryPool {
        std::vector<void*> free_buffers;
        std::vector<void*> used_buffers;
        size_t buffer_size;
        size_t pool_size;
    };
    
    MemoryPool video_memory_pool_;
    MemoryPool audio_memory_pool_;
    
    // 初始化内存池
    bool initializeMemoryPool(MemoryPool& pool, size_t buffer_size, size_t pool_size);
    
    // 从内存池获取缓冲区
    void* getBufferFromPool(MemoryPool& pool);
    void returnBufferToPool(MemoryPool& pool, void* buffer);
    
    // 同步GPU内存
    void synchronizeGPUMemory(void* gpu_buffer);
    bool waitForGPUMemory(void* gpu_buffer);
};

} // namespace StreamKit 