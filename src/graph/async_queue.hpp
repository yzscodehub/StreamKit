/**
 * @file async_queue.hpp
 * @brief AsyncQueueNode provides thread boundaries in the pipeline
 * 
 * CRITICAL REQUIREMENTS:
 * 1. Destructor MUST call stop() to prevent thread join deadlock
 * 2. Must expose flush() for seek operations
 * 3. Must wake blocked push()/pop() on shutdown
 * 4. Uses "Poison Pill" pattern - EOF marker propagates through
 */

#pragma once

#include <thread>
#include <atomic>
#include <functional>

#include "concepts.hpp"
#include "pin.hpp"
#include "node.hpp"
#include "core/types.hpp"

#include <spdlog/spdlog.h>

namespace phoenix {

/**
 * @brief Thread boundary node with internal worker thread
 * 
 * AsyncQueueNode creates a thread that pulls from input and pushes to output.
 * This provides natural backpressure and thread isolation.
 * 
 * Pipeline example:
 *   SourceNode -> [AsyncQueue] -> DecoderNode -> [AsyncQueue] -> SinkNode
 *                 ^ thread        ^ same thread   ^ thread
 * 
 * @tparam T Data type flowing through
 */
template<Transferable T>
class AsyncQueueNode : public NodeBase {
public:
    using DataType = T;
    
    /**
     * @brief Construct AsyncQueueNode
     * @param name Node name for debugging
     * @param inputCapacity Input queue capacity
     * @param outputCapacity Output queue capacity (usually same as input)
     */
    explicit AsyncQueueNode(std::string name,
                           size_t inputCapacity = kDefaultVideoQueueCapacity,
                           size_t outputCapacity = 0)
        : NodeBase(std::move(name))
        , input(inputCapacity)
    {
        // Output capacity defaults to input capacity if not specified
        (void)outputCapacity;  // Output connects to external InputPin
    }
    
    /**
     * @brief Destructor - MUST call stop() to prevent deadlock
     */
    ~AsyncQueueNode() override {
        stop();
    }
    
    // Non-copyable, non-movable (owns thread)
    AsyncQueueNode(const AsyncQueueNode&) = delete;
    AsyncQueueNode& operator=(const AsyncQueueNode&) = delete;
    AsyncQueueNode(AsyncQueueNode&&) = delete;
    AsyncQueueNode& operator=(AsyncQueueNode&&) = delete;
    
    /// Input pin
    InputPin<T> input;
    
    /// Output pin
    OutputPin<T> output;
    
    // ========== Lifecycle ==========
    
    /**
     * @brief Start the worker thread
     */
    void start() override {
        if (running_.exchange(true, std::memory_order_acq_rel)) {
            return;  // Already running
        }
        
        input.reset();  // Clear stopped flag
        
        worker_ = std::thread([this] {
            workerLoop();
        });
        
        spdlog::debug("[{}] Started", name_);
    }
    
    /**
     * @brief Stop the worker thread
     * 
     * CRITICAL: This must wake all blocked threads to prevent join deadlock.
     */
    void stop() override {
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;  // Already stopped
        }
        
        // Wake blocked threads
        input.stop();
        
        // Join worker
        if (worker_.joinable()) {
            worker_.join();
        }
        
        spdlog::debug("[{}] Stopped", name_);
    }
    
    /**
     * @brief Flush all pending data
     * 
     * Used during seek operations. Discards all queued items.
     */
    void flush() override {
        input.flush();
        spdlog::debug("[{}] Flushed", name_);
    }
    
    /**
     * @brief Reset after seek
     */
    void reset() {
        input.reset();
    }
    
    // ========== Statistics ==========
    
    [[nodiscard]] size_t queueSize() const {
        return input.size();
    }
    
    [[nodiscard]] uint64_t itemsProcessed() const {
        return itemsProcessed_.load(std::memory_order_relaxed);
    }
    
private:
    /**
     * @brief Worker thread loop
     * 
     * Continuously pops from input and pushes to output.
     * Handles EOF by propagating it downstream.
     */
    void workerLoop() {
        spdlog::debug("[{}] Worker started", name_);
        
        while (running_.load(std::memory_order_acquire)) {
            // Pop from input (blocking)
            auto [result, data] = input.pop(std::chrono::milliseconds(100));
            
            if (result == PopResult::Terminated) {
                spdlog::debug("[{}] Received termination signal", name_);
                break;
            }
            
            if (result == PopResult::Timeout) {
                // Check if still running and retry
                continue;
            }
            
            if (!data.has_value()) {
                continue;
            }
            
            // Check for EOF marker
            bool isEof = false;
            if constexpr (HasEof<T>) {
                isEof = data->isEof();
            }
            
            // Push to output
            if (output.isConnected()) {
                auto pushResult = output.emit(std::move(*data));
                if (!pushResult.ok()) {
                    if (running_.load(std::memory_order_acquire)) {
                        spdlog::warn("[{}] Failed to emit: {}", name_, 
                            pushResult.error().what());
                    }
                    break;
                }
            }
            
            itemsProcessed_.fetch_add(1, std::memory_order_relaxed);
            
            // If EOF, we can stop (but don't break - let Pipeline handle shutdown)
            if (isEof) {
                spdlog::debug("[{}] Propagated EOF", name_);
            }
        }
        
        spdlog::debug("[{}] Worker exited, processed {} items", 
            name_, itemsProcessed_.load());
    }
    
    std::thread worker_;
    std::atomic<uint64_t> itemsProcessed_{0};
};

// ============================================================================
// Helper: Create typed AsyncQueueNode
// ============================================================================

template<Transferable T>
inline std::unique_ptr<AsyncQueueNode<T>> makeAsyncQueue(
    std::string name,
    size_t capacity = kDefaultVideoQueueCapacity)
{
    return std::make_unique<AsyncQueueNode<T>>(std::move(name), capacity);
}

} // namespace phoenix

