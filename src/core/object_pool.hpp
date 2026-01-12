/**
 * @file object_pool.hpp
 * @brief Thread-safe object pool for VideoFrame recycling
 * 
 * Provides efficient memory reuse by recycling objects instead of
 * frequent allocation/deallocation.
 */

#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <optional>
#include <chrono>

namespace phoenix {

/**
 * @brief Thread-safe object pool with bounded capacity
 * 
 * Objects can be acquired (blocking or non-blocking) and released back.
 * When the pool is empty, acquire() blocks until an object is available.
 * 
 * @tparam T Object type (must be default constructible)
 */
template<typename T>
class ObjectPool {
public:
    /// Recycler function type - returns object to pool
    using Recycler = std::function<void(T)>;
    
    /**
     * @brief Construct pool with initial capacity
     * @param initialCapacity Number of objects to pre-allocate
     * @param maxCapacity Maximum pool size (0 = no limit)
     */
    explicit ObjectPool(size_t initialCapacity = 10, size_t maxCapacity = 0)
        : maxCapacity_(maxCapacity == 0 ? SIZE_MAX : maxCapacity)
    {
        for (size_t i = 0; i < initialCapacity; ++i) {
            pool_.push(T{});
        }
    }
    
    // Non-copyable, non-movable
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;
    
    // ========== Acquire Operations ==========
    
    /**
     * @brief Acquire object from pool (blocking)
     * @param timeout Maximum wait time (0 = wait forever)
     * @return Object if available, nullopt on timeout
     */
    std::optional<T> acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock lock(mutex_);
        
        if (timeout.count() == 0) {
            // Wait indefinitely
            cv_.wait(lock, [this] { return !pool_.empty() || stopped_; });
        } else {
            // Wait with timeout
            if (!cv_.wait_for(lock, timeout, [this] { return !pool_.empty() || stopped_; })) {
                return std::nullopt;  // Timeout
            }
        }
        
        if (stopped_) {
            return std::nullopt;
        }
        
        T obj = std::move(pool_.front());
        pool_.pop();
        return obj;
    }
    
    /**
     * @brief Try to acquire without blocking
     * @return Object if available, nullopt if pool empty
     */
    std::optional<T> tryAcquire() {
        std::lock_guard lock(mutex_);
        
        if (pool_.empty()) {
            return std::nullopt;
        }
        
        T obj = std::move(pool_.front());
        pool_.pop();
        return obj;
    }
    
    /**
     * @brief Acquire or create new object
     * 
     * If pool is empty, creates a new object (if under max capacity).
     */
    T acquireOrCreate() {
        std::lock_guard lock(mutex_);
        
        if (!pool_.empty()) {
            T obj = std::move(pool_.front());
            pool_.pop();
            return obj;
        }
        
        // Create new object
        return T{};
    }
    
    // ========== Release Operations ==========
    
    /**
     * @brief Release object back to pool
     * @param obj Object to release
     */
    void release(T obj) {
        {
            std::lock_guard lock(mutex_);
            
            if (pool_.size() < maxCapacity_) {
                pool_.push(std::move(obj));
            }
            // If over capacity, object is simply destroyed
        }
        cv_.notify_one();
    }
    
    /**
     * @brief Get a recycler function for this pool
     * 
     * Usage:
     *   auto recycler = pool.getRecycler();
     *   recycler(frame);  // Returns frame to pool
     */
    Recycler getRecycler() {
        return [this](T obj) {
            this->release(std::move(obj));
        };
    }
    
    // ========== Pool Management ==========
    
    /**
     * @brief Clear all objects from pool
     */
    void clear() {
        std::lock_guard lock(mutex_);
        std::queue<T> empty;
        std::swap(pool_, empty);
    }
    
    /**
     * @brief Stop the pool (wake up all waiters)
     */
    void stop() {
        {
            std::lock_guard lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
    }
    
    /**
     * @brief Reset pool to running state
     */
    void reset() {
        std::lock_guard lock(mutex_);
        stopped_ = false;
    }
    
    // ========== State Queries ==========
    
    /**
     * @brief Get current number of available objects
     */
    [[nodiscard]] size_t available() const {
        std::lock_guard lock(mutex_);
        return pool_.size();
    }
    
    /**
     * @brief Check if pool is empty
     */
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return pool_.empty();
    }
    
    /**
     * @brief Check if pool is stopped
     */
    [[nodiscard]] bool stopped() const {
        std::lock_guard lock(mutex_);
        return stopped_;
    }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> pool_;
    size_t maxCapacity_;
    bool stopped_ = false;
};

/**
 * @brief RAII handle that automatically returns object to pool
 * 
 * Usage:
 *   auto handle = pool.acquireHandle();
 *   handle->doSomething();  // Use object
 *   // Object automatically returned when handle goes out of scope
 */
template<typename T>
class PoolHandle {
public:
    PoolHandle() = default;
    
    PoolHandle(T obj, std::function<void(T)> releaser)
        : obj_(std::move(obj))
        , releaser_(std::move(releaser))
        , valid_(true)
    {}
    
    ~PoolHandle() {
        if (valid_ && releaser_) {
            releaser_(std::move(*obj_));
        }
    }
    
    // Move only
    PoolHandle(PoolHandle&& other) noexcept
        : obj_(std::move(other.obj_))
        , releaser_(std::move(other.releaser_))
        , valid_(other.valid_)
    {
        other.valid_ = false;
    }
    
    PoolHandle& operator=(PoolHandle&& other) noexcept {
        if (this != &other) {
            if (valid_ && releaser_) {
                releaser_(std::move(*obj_));
            }
            obj_ = std::move(other.obj_);
            releaser_ = std::move(other.releaser_);
            valid_ = other.valid_;
            other.valid_ = false;
        }
        return *this;
    }
    
    PoolHandle(const PoolHandle&) = delete;
    PoolHandle& operator=(const PoolHandle&) = delete;
    
    // Accessors
    T* operator->() { return &*obj_; }
    const T* operator->() const { return &*obj_; }
    T& operator*() { return *obj_; }
    const T& operator*() const { return *obj_; }
    
    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] explicit operator bool() const { return valid_; }
    
    /// Release ownership without returning to pool
    T release() {
        valid_ = false;
        return std::move(*obj_);
    }
    
private:
    std::optional<T> obj_;
    std::function<void(T)> releaser_;
    bool valid_ = false;
};

} // namespace phoenix

