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
        : m_maxCapacity(maxCapacity == 0 ? SIZE_MAX : maxCapacity)
    {
        for (size_t i = 0; i < initialCapacity; ++i) {
            m_pool.push(T{});
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
        std::unique_lock lock(m_mutex);
        
        if (timeout.count() == 0) {
            m_cv.wait(lock, [this] { return !m_pool.empty() || m_stopped; });
        } else {
            if (!m_cv.wait_for(lock, timeout, [this] { return !m_pool.empty() || m_stopped; })) {
                return std::nullopt;
            }
        }
        
        if (m_stopped) {
            return std::nullopt;
        }
        
        T obj = std::move(m_pool.front());
        m_pool.pop();
        return obj;
    }
    
    /**
     * @brief Try to acquire without blocking
     */
    std::optional<T> tryAcquire() {
        std::lock_guard lock(m_mutex);
        
        if (m_pool.empty()) {
            return std::nullopt;
        }
        
        T obj = std::move(m_pool.front());
        m_pool.pop();
        return obj;
    }
    
    /**
     * @brief Acquire or create new object
     */
    T acquireOrCreate() {
        std::lock_guard lock(m_mutex);
        
        if (!m_pool.empty()) {
            T obj = std::move(m_pool.front());
            m_pool.pop();
            return obj;
        }
        
        return T{};
    }
    
    // ========== Release Operations ==========
    
    /**
     * @brief Release object back to pool
     */
    void release(T obj) {
        {
            std::lock_guard lock(m_mutex);
            
            if (m_pool.size() < m_maxCapacity) {
                m_pool.push(std::move(obj));
            }
        }
        m_cv.notify_one();
    }
    
    /**
     * @brief Get a recycler function for this pool
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
        std::lock_guard lock(m_mutex);
        std::queue<T> empty;
        std::swap(m_pool, empty);
    }
    
    /**
     * @brief Stop the pool (wake up all waiters)
     */
    void stop() {
        {
            std::lock_guard lock(m_mutex);
            m_stopped = true;
        }
        m_cv.notify_all();
    }
    
    /**
     * @brief Reset pool to running state
     */
    void reset() {
        std::lock_guard lock(m_mutex);
        m_stopped = false;
    }
    
    // ========== State Queries ==========
    
    [[nodiscard]] size_t available() const {
        std::lock_guard lock(m_mutex);
        return m_pool.size();
    }
    
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(m_mutex);
        return m_pool.empty();
    }
    
    [[nodiscard]] bool stopped() const {
        std::lock_guard lock(m_mutex);
        return m_stopped;
    }
    
private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<T> m_pool;
    size_t m_maxCapacity;
    bool m_stopped = false;
};

/**
 * @brief RAII handle that automatically returns object to pool
 */
template<typename T>
class PoolHandle {
public:
    PoolHandle() = default;
    
    PoolHandle(T obj, std::function<void(T)> releaser)
        : m_obj(std::move(obj))
        , m_releaser(std::move(releaser))
        , m_valid(true)
    {}
    
    ~PoolHandle() {
        if (m_valid && m_releaser) {
            m_releaser(std::move(*m_obj));
        }
    }
    
    // Move only
    PoolHandle(PoolHandle&& other) noexcept
        : m_obj(std::move(other.m_obj))
        , m_releaser(std::move(other.m_releaser))
        , m_valid(other.m_valid)
    {
        other.m_valid = false;
    }
    
    PoolHandle& operator=(PoolHandle&& other) noexcept {
        if (this != &other) {
            if (m_valid && m_releaser) {
                m_releaser(std::move(*m_obj));
            }
            m_obj = std::move(other.m_obj);
            m_releaser = std::move(other.m_releaser);
            m_valid = other.m_valid;
            other.m_valid = false;
        }
        return *this;
    }
    
    PoolHandle(const PoolHandle&) = delete;
    PoolHandle& operator=(const PoolHandle&) = delete;
    
    // Accessors
    T* operator->() { return &*m_obj; }
    const T* operator->() const { return &*m_obj; }
    T& operator*() { return *m_obj; }
    const T& operator*() const { return *m_obj; }
    
    [[nodiscard]] bool valid() const { return m_valid; }
    [[nodiscard]] explicit operator bool() const { return m_valid; }
    
    /// Release ownership without returning to pool
    T release() {
        m_valid = false;
        return std::move(*m_obj);
    }
    
private:
    std::optional<T> m_obj;
    std::function<void(T)> m_releaser;
    bool m_valid = false;
};

} // namespace phoenix
