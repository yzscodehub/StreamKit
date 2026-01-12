/**
 * @file pin.hpp
 * @brief InputPin and OutputPin with backpressure support
 * 
 * InputPin: Bounded queue with blocking push/pop
 * - Blocks producer when full (backpressure)
 * - Blocks consumer when empty
 * - Supports flush() for seek operations
 * - Supports stop() to wake all blocked threads
 * 
 * OutputPin: Simple wrapper that forwards to connected InputPin
 */

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <atomic>

#include "concepts.hpp"
#include "core/types.hpp"
#include "core/result.hpp"

namespace phoenix {

// ============================================================================
// PopResult - Distinguishes timeout vs termination
// ============================================================================

enum class PopResult {
    Ok,         // Got data successfully
    Timeout,    // Timed out waiting
    Terminated  // Queue was stopped
};

// ============================================================================
// InputPin - Bounded queue with backpressure
// ============================================================================

/**
 * @brief Thread-safe bounded queue with blocking operations
 * 
 * Provides backpressure: producer blocks when queue is full.
 * Critical for memory management in video pipelines.
 * 
 * @tparam T Data type (must satisfy Transferable concept)
 */
template<Transferable T>
class InputPin {
public:
    /**
     * @brief Construct InputPin with given capacity
     * @param capacity Maximum queue size (0 = unbounded)
     */
    explicit InputPin(size_t capacity = 50)
        : capacity_(capacity == 0 ? SIZE_MAX : capacity)
    {}
    
    // Non-copyable, non-movable
    InputPin(const InputPin&) = delete;
    InputPin& operator=(const InputPin&) = delete;
    InputPin(InputPin&&) = delete;
    InputPin& operator=(InputPin&&) = delete;
    
    // ========== Producer Interface ==========
    
    /**
     * @brief Push data to queue (blocking if full)
     * 
     * @param data Data to push
     * @param timeout Maximum wait time (0 = wait forever)
     * @return Ok on success, Timeout on timeout, error on stopped
     */
    Result<void> push(T data, std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock lock(mutex_);
        
        // Wait for space
        auto predicate = [this] { return queue_.size() < capacity_ || stopped_; };
        
        if (timeout.count() == 0) {
            cv_space_.wait(lock, predicate);
        } else {
            if (!cv_space_.wait_for(lock, timeout, predicate)) {
                return Err(ErrorCode::Timeout, "Push timeout");
            }
        }
        
        if (stopped_) {
            return Err(ErrorCode::Terminated, "Queue stopped");
        }
        
        queue_.push(std::move(data));
        lock.unlock();
        cv_data_.notify_one();
        
        return Ok();
    }
    
    /**
     * @brief Try to push without blocking
     * @return true if pushed, false if queue full
     */
    bool tryPush(T data) {
        std::lock_guard lock(mutex_);
        
        if (queue_.size() >= capacity_ || stopped_) {
            return false;
        }
        
        queue_.push(std::move(data));
        cv_data_.notify_one();
        return true;
    }
    
    // ========== Consumer Interface ==========
    
    /**
     * @brief Pop data from queue (blocking if empty)
     * 
     * @param timeout Maximum wait time (0 = wait forever)
     * @return Pair of (PopResult, optional data)
     */
    std::pair<PopResult, std::optional<T>> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock lock(mutex_);
        
        // Wait for data
        auto predicate = [this] { return !queue_.empty() || stopped_; };
        
        if (timeout.count() == 0) {
            cv_data_.wait(lock, predicate);
        } else {
            if (!cv_data_.wait_for(lock, timeout, predicate)) {
                return {PopResult::Timeout, std::nullopt};
            }
        }
        
        if (stopped_ && queue_.empty()) {
            return {PopResult::Terminated, std::nullopt};
        }
        
        if (queue_.empty()) {
            return {PopResult::Terminated, std::nullopt};
        }
        
        T data = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        cv_space_.notify_one();
        
        return {PopResult::Ok, std::move(data)};
    }
    
    /**
     * @brief Try to pop without blocking
     * @return Data if available, nullopt if empty
     */
    std::optional<T> tryPop() {
        std::lock_guard lock(mutex_);
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T data = std::move(queue_.front());
        queue_.pop();
        cv_space_.notify_one();
        return data;
    }
    
    /**
     * @brief Peek at front item without removing
     * @return Pointer to front item, nullptr if empty
     */
    const T* peek() const {
        std::lock_guard lock(mutex_);
        return queue_.empty() ? nullptr : &queue_.front();
    }
    
    // ========== Control Interface ==========
    
    /**
     * @brief Flush queue (discard all pending items)
     * 
     * Used during seek operations. Does NOT notify waiters.
     */
    void flush() {
        std::lock_guard lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
        // Note: Don't notify - we're resetting, not producing
    }
    
    /**
     * @brief Stop the queue and wake all waiters
     * 
     * CRITICAL: Must be called before destruction if threads may be waiting.
     */
    void stop() {
        {
            std::lock_guard lock(mutex_);
            stopped_ = true;
        }
        // Wake ALL waiters
        cv_data_.notify_all();
        cv_space_.notify_all();
    }
    
    /**
     * @brief Reset queue to running state
     * 
     * Clears queue and resets stopped flag. Used after seek.
     */
    void reset() {
        std::lock_guard lock(mutex_);
        stopped_ = false;
        std::queue<T> empty;
        std::swap(queue_, empty);
    }
    
    // ========== State Queries ==========
    
    [[nodiscard]] size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
    
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }
    
    [[nodiscard]] bool full() const {
        std::lock_guard lock(mutex_);
        return queue_.size() >= capacity_;
    }
    
    [[nodiscard]] size_t capacity() const {
        return capacity_;
    }
    
    [[nodiscard]] bool isStopped() const {
        std::lock_guard lock(mutex_);
        return stopped_;
    }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_space_;  // Signals when space available
    std::condition_variable cv_data_;   // Signals when data available
    std::queue<T> queue_;
    size_t capacity_;
    bool stopped_ = false;
};

// ============================================================================
// OutputPin - Forwards to connected InputPin
// ============================================================================

/**
 * @brief Output pin that forwards data to a connected InputPin
 * 
 * Simple wrapper that maintains a connection to an InputPin.
 * 
 * @tparam T Data type
 */
template<Transferable T>
class OutputPin {
public:
    OutputPin() = default;
    
    // Non-copyable
    OutputPin(const OutputPin&) = delete;
    OutputPin& operator=(const OutputPin&) = delete;
    
    // Movable
    OutputPin(OutputPin&& other) noexcept : target_(other.target_) {
        other.target_ = nullptr;
    }
    
    OutputPin& operator=(OutputPin&& other) noexcept {
        target_ = other.target_;
        other.target_ = nullptr;
        return *this;
    }
    
    /**
     * @brief Connect to an InputPin
     */
    void connect(InputPin<T>* target) {
        target_ = target;
    }
    
    /**
     * @brief Disconnect from InputPin
     */
    void disconnect() {
        target_ = nullptr;
    }
    
    /**
     * @brief Check if connected
     */
    [[nodiscard]] bool isConnected() const {
        return target_ != nullptr;
    }
    
    /**
     * @brief Emit data to connected InputPin
     * 
     * @param data Data to emit
     * @param timeout Push timeout
     * @return Ok on success, error on failure
     */
    Result<void> emit(T data, std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        if (!target_) {
            return Err(ErrorCode::InvalidArgument, "OutputPin not connected");
        }
        return target_->push(std::move(data), timeout);
    }
    
    /**
     * @brief Try to emit without blocking
     */
    bool tryEmit(T data) {
        if (!target_) {
            return false;
        }
        return target_->tryPush(std::move(data));
    }
    
    /**
     * @brief Get connected InputPin
     */
    InputPin<T>* target() { return target_; }
    const InputPin<T>* target() const { return target_; }
    
private:
    InputPin<T>* target_ = nullptr;
};

} // namespace phoenix

