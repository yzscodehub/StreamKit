#include "cuda/cuda_processor.hpp"
#include "common/logger.hpp"
#include <cuda_runtime.h>
#include <libavutil/imgutils.h>

namespace StreamKit {

CudaProcessor::CudaProcessor()
    : video_stream_(0)
    , audio_stream_(0)
    , video_event_(0)
    , audio_event_(0)
    , device_id_(0)
    , initialized_(false) {
    
    memset(&device_prop_, 0, sizeof(device_prop_));
}

CudaProcessor::~CudaProcessor() {
    cleanup();
}

bool CudaProcessor::initialize(int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("CUDA processor already initialized");
        return true;
    }
    
    device_id_ = device_id;
    
    // 设置CUDA设备
    cudaError_t err = cudaSetDevice(device_id_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to set CUDA device {}: {}", device_id_, cudaGetErrorString(err));
        return false;
    }
    
    // 获取设备属性
    err = cudaGetDeviceProperties(&device_prop_, device_id_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to get device properties: {}", cudaGetErrorString(err));
        return false;
    }
    
    // 创建CUDA流
    err = cudaStreamCreate(&video_stream_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create video stream: {}", cudaGetErrorString(err));
        return false;
    }
    
    err = cudaStreamCreate(&audio_stream_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create audio stream: {}", cudaGetErrorString(err));
        cudaStreamDestroy(video_stream_);
        return false;
    }
    
    // 创建CUDA事件
    err = cudaEventCreate(&video_event_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create video event: {}", cudaGetErrorString(err));
        cudaStreamDestroy(video_stream_);
        cudaStreamDestroy(audio_stream_);
        return false;
    }
    
    err = cudaEventCreate(&audio_event_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create audio event: {}", cudaGetErrorString(err));
        cudaEventDestroy(video_event_);
        cudaStreamDestroy(video_stream_);
        cudaStreamDestroy(audio_stream_);
        return false;
    }
    
    // 初始化内存池
    size_t video_buffer_size = 1920 * 1080 * 3 / 2;  // NV12格式
    size_t audio_buffer_size = 48000 * 2 * 2;  // 48kHz, 2ch, 16bit
    
    if (!initializeMemoryPool(video_memory_pool_, video_buffer_size, 10)) {
        LOG_ERROR("Failed to initialize video memory pool");
        return false;
    }
    
    if (!initializeMemoryPool(audio_memory_pool_, audio_buffer_size, 20)) {
        LOG_ERROR("Failed to initialize audio memory pool");
        return false;
    }
    
    initialized_ = true;
    
    LOG_INFO("CUDA processor initialized on device {}: {}", 
             device_id_, device_prop_.name);
    LOG_INFO("Compute capability: {}.{}", device_prop_.major, device_prop_.minor);
    LOG_INFO("Global memory: {} MB", device_prop_.totalGlobalMem / (1024 * 1024));
    
    return true;
}

bool CudaProcessor::processVideoFrame(AVFrame* input_frame, AVFrame* output_frame) {
    if (!initialized_ || !input_frame || !output_frame) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取GPU内存
    void* gpu_input = getBufferFromPool(video_memory_pool_);
    void* gpu_output = getBufferFromPool(video_memory_pool_);
    
    if (!gpu_input || !gpu_output) {
        LOG_ERROR("Failed to get GPU memory for video processing");
        return false;
    }
    
    // 计算帧大小
    size_t frame_size = av_image_get_buffer_size(
        (AVPixelFormat)input_frame->format, 
        input_frame->width, 
        input_frame->height, 
        1);
    
    // 拷贝数据到GPU
    cudaError_t err = cudaMemcpyAsync(gpu_input, input_frame->data[0], frame_size,
                                     cudaMemcpyHostToDevice, video_stream_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy video data to GPU: {}", cudaGetErrorString(err));
        returnBufferToPool(video_memory_pool_, gpu_input);
        returnBufferToPool(video_memory_pool_, gpu_output);
        return false;
    }
    
    // 简单的色彩空间转换（这里只是示例，实际需要更复杂的处理）
    // 在实际应用中，这里会调用CUDA kernel进行色彩空间转换、缩放等操作
    
    // 拷贝结果回CPU
    err = cudaMemcpyAsync(output_frame->data[0], gpu_output, frame_size,
                         cudaMemcpyDeviceToHost, video_stream_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy video data from GPU: {}", cudaGetErrorString(err));
        returnBufferToPool(video_memory_pool_, gpu_input);
        returnBufferToPool(video_memory_pool_, gpu_output);
        return false;
    }
    
    // 记录事件
    cudaEventRecord(video_event_, video_stream_);
    
    // 返回内存到池
    returnBufferToPool(video_memory_pool_, gpu_input);
    returnBufferToPool(video_memory_pool_, gpu_output);
    
    return true;
}

bool CudaProcessor::processAudioData(const uint8_t* input, uint8_t* output, int samples) {
    if (!initialized_ || !input || !output || samples <= 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取GPU内存
    void* gpu_input = getBufferFromPool(audio_memory_pool_);
    void* gpu_output = getBufferFromPool(audio_memory_pool_);
    
    if (!gpu_input || !gpu_output) {
        LOG_ERROR("Failed to get GPU memory for audio processing");
        return false;
    }
    
    size_t data_size = samples * 4;  // 假设16bit stereo
    
    // 拷贝数据到GPU
    cudaError_t err = cudaMemcpyAsync(gpu_input, input, data_size,
                                     cudaMemcpyHostToDevice, audio_stream_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy audio data to GPU: {}", cudaGetErrorString(err));
        returnBufferToPool(audio_memory_pool_, gpu_input);
        returnBufferToPool(audio_memory_pool_, gpu_output);
        return false;
    }
    
    // 音频处理（这里只是示例）
    // 在实际应用中，这里会调用CUDA kernel进行音频处理
    
    // 拷贝结果回CPU
    err = cudaMemcpyAsync(output, gpu_output, data_size,
                         cudaMemcpyDeviceToHost, audio_stream_);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy audio data from GPU: {}", cudaGetErrorString(err));
        returnBufferToPool(audio_memory_pool_, gpu_input);
        returnBufferToPool(audio_memory_pool_, gpu_output);
        return false;
    }
    
    // 记录事件
    cudaEventRecord(audio_event_, audio_stream_);
    
    // 返回内存到池
    returnBufferToPool(audio_memory_pool_, gpu_input);
    returnBufferToPool(audio_memory_pool_, gpu_output);
    
    return true;
}

void CudaProcessor::asyncProcessVideoFrame(AVFrame* input_frame, AVFrame* output_frame) {
    // 异步处理视频帧
    std::thread([this, input_frame, output_frame]() {
        processVideoFrame(input_frame, output_frame);
    }).detach();
}

void CudaProcessor::asyncProcessAudioData(const uint8_t* input, uint8_t* output, int samples) {
    // 异步处理音频数据
    std::thread([this, input, output, samples]() {
        processAudioData(input, output, samples);
    }).detach();
}

void CudaProcessor::waitForVideoProcessing() {
    if (initialized_) {
        cudaEventSynchronize(video_event_);
    }
}

void CudaProcessor::waitForAudioProcessing() {
    if (initialized_) {
        cudaEventSynchronize(audio_event_);
    }
}

void CudaProcessor::synchronize() {
    if (initialized_) {
        cudaDeviceSynchronize();
    }
}

void* CudaProcessor::allocateGPUMemory(size_t size) {
    if (!initialized_) {
        return nullptr;
    }
    
    void* ptr = nullptr;
    cudaError_t err = cudaMalloc(&ptr, size);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate GPU memory: {}", cudaGetErrorString(err));
        return nullptr;
    }
    
    // 注册到同步管理器
    GPUMemorySync sync;
    sync.gpu_buffer = ptr;
    sync.size = size;
    sync.is_ready = true;
    
    std::lock_guard<std::mutex> lock(mutex_);
    gpu_memory_sync_[ptr] = sync;
    
    return ptr;
}

void CudaProcessor::freeGPUMemory(void* ptr) {
    if (!ptr || !initialized_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    gpu_memory_sync_.erase(ptr);
    
    cudaFree(ptr);
}

bool CudaProcessor::copyToGPU(const void* host_ptr, void* device_ptr, size_t size) {
    if (!initialized_ || !host_ptr || !device_ptr) {
        return false;
    }
    
    cudaError_t err = cudaMemcpy(device_ptr, host_ptr, size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy data to GPU: {}", cudaGetErrorString(err));
        return false;
    }
    
    return true;
}

bool CudaProcessor::copyFromGPU(const void* device_ptr, void* host_ptr, size_t size) {
    if (!initialized_ || !device_ptr || !host_ptr) {
        return false;
    }
    
    cudaError_t err = cudaMemcpy(host_ptr, device_ptr, size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy data from GPU: {}", cudaGetErrorString(err));
        return false;
    }
    
    return true;
}

bool CudaProcessor::isGPUReady() const {
    if (!initialized_) {
        return false;
    }
    
    cudaError_t err = cudaGetLastError();
    return err == cudaSuccess;
}

void CudaProcessor::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        // 清理内存池
        for (void* ptr : video_memory_pool_.free_buffers) {
            cudaFree(ptr);
        }
        for (void* ptr : video_memory_pool_.used_buffers) {
            cudaFree(ptr);
        }
        
        for (void* ptr : audio_memory_pool_.free_buffers) {
            cudaFree(ptr);
        }
        for (void* ptr : audio_memory_pool_.used_buffers) {
            cudaFree(ptr);
        }
        
        // 清理GPU内存同步
        for (auto& pair : gpu_memory_sync_) {
            cudaFree(pair.first);
        }
        gpu_memory_sync_.clear();
        
        // 销毁CUDA对象
        if (video_event_) {
            cudaEventDestroy(video_event_);
        }
        if (audio_event_) {
            cudaEventDestroy(audio_event_);
        }
        if (video_stream_) {
            cudaStreamDestroy(video_stream_);
        }
        if (audio_stream_) {
            cudaStreamDestroy(audio_stream_);
        }
        
        initialized_ = false;
        
        LOG_INFO("CUDA processor cleaned up");
    }
}

bool CudaProcessor::initializeMemoryPool(MemoryPool& pool, size_t buffer_size, size_t pool_size) {
    pool.buffer_size = buffer_size;
    pool.pool_size = pool_size;
    
    for (size_t i = 0; i < pool_size; ++i) {
        void* buffer = nullptr;
        cudaError_t err = cudaMalloc(&buffer, buffer_size);
        if (err != cudaSuccess) {
            LOG_ERROR("Failed to allocate GPU memory for pool: {}", cudaGetErrorString(err));
            // 清理已分配的内存
            for (void* ptr : pool.free_buffers) {
                cudaFree(ptr);
            }
            pool.free_buffers.clear();
            return false;
        }
        pool.free_buffers.push_back(buffer);
    }
    
    return true;
}

void* CudaProcessor::getBufferFromPool(MemoryPool& pool) {
    if (pool.free_buffers.empty()) {
        return nullptr;
    }
    
    void* buffer = pool.free_buffers.back();
    pool.free_buffers.pop_back();
    pool.used_buffers.push_back(buffer);
    
    return buffer;
}

void CudaProcessor::returnBufferToPool(MemoryPool& pool, void* buffer) {
    if (!buffer) {
        return;
    }
    
    // 从已使用列表中移除
    auto it = std::find(pool.used_buffers.begin(), pool.used_buffers.end(), buffer);
    if (it != pool.used_buffers.end()) {
        pool.used_buffers.erase(it);
    }
    
    // 添加到空闲列表
    pool.free_buffers.push_back(buffer);
}

void CudaProcessor::synchronizeGPUMemory(void* gpu_buffer) {
    auto it = gpu_memory_sync_.find(gpu_buffer);
    if (it != gpu_memory_sync_.end()) {
        cudaEventSynchronize(it->second.ready_event);
        it->second.is_ready = true;
    }
}

bool CudaProcessor::waitForGPUMemory(void* gpu_buffer) {
    auto it = gpu_memory_sync_.find(gpu_buffer);
    if (it != gpu_memory_sync_.end()) {
        cudaEventSynchronize(it->second.ready_event);
        return it->second.is_ready;
    }
    return false;
}

} // namespace StreamKit 