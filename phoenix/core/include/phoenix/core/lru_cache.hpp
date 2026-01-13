/**
 * @file lru_cache.hpp
 * @brief Thread-safe LRU (Least Recently Used) cache template
 * 
 * Used for caching decoded frames, thumbnails, and other expensive
 * resources that benefit from reuse.
 * 
 * Features:
 * - O(1) get and put operations
 * - Thread-safe with shared mutex (multiple readers, single writer)
 * - Configurable maximum size
 * - Optional eviction callback
 */

#pragma once

#include <unordered_map>
#include <list>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <memory>

namespace phoenix {

/**
 * @brief Thread-safe LRU cache
 * 
 * @tparam Key Key type (must be hashable)
 * @tparam Value Value type
 * 
 * Usage:
 * @code
 *   LRUCache<std::string, VideoFrame> cache(100);  // 100 frames max
 *   
 *   cache.put("frame_001", frame);
 *   
 *   if (auto frame = cache.get("frame_001")) {
 *       // Use cached frame
 *   }
 * @endcode
 */
template<typename Key, typename Value>
class LRUCache {
public:
    using EvictionCallback = std::function<void(const Key&, Value&)>;
    
    /**
     * @brief Construct cache with maximum capacity
     * 
     * @param maxSize Maximum number of items to cache
     * @param onEvict Optional callback when items are evicted
     */
    explicit LRUCache(size_t maxSize, EvictionCallback onEvict = nullptr)
        : m_maxSize(maxSize)
        , m_onEvict(std::move(onEvict))
    {}
    
    // Non-copyable (contains mutex)
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    
    // Movable
    LRUCache(LRUCache&& other) noexcept {
        std::unique_lock lock(other.m_mutex);
        m_maxSize = other.m_maxSize;
        m_list = std::move(other.m_list);
        m_map = std::move(other.m_map);
        m_onEvict = std::move(other.m_onEvict);
    }
    
    LRUCache& operator=(LRUCache&& other) noexcept {
        if (this != &other) {
            std::scoped_lock lock(m_mutex, other.m_mutex);
            m_maxSize = other.m_maxSize;
            m_list = std::move(other.m_list);
            m_map = std::move(other.m_map);
            m_onEvict = std::move(other.m_onEvict);
        }
        return *this;
    }
    
    /**
     * @brief Get a value from the cache
     * 
     * If found, the item is moved to the front (most recently used).
     * 
     * @param key Key to look up
     * @return Optional containing the value, or nullopt if not found
     */
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        std::unique_lock lock(m_mutex);
        
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return std::nullopt;
        }
        
        // Move to front (most recently used)
        m_list.splice(m_list.begin(), m_list, it->second);
        
        return it->second->second;
    }
    
    /**
     * @brief Get a pointer to the value (does not copy)
     * 
     * @param key Key to look up
     * @return Pointer to value, or nullptr if not found
     * 
     * @warning The pointer is only valid while holding the cache.
     *          Use get() for thread-safe access to copied value.
     */
    [[nodiscard]] const Value* peek(const Key& key) const {
        std::shared_lock lock(m_mutex);
        
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return nullptr;
        }
        
        return &(it->second->second);
    }
    
    /**
     * @brief Check if a key exists in the cache
     * 
     * Does not affect LRU ordering.
     */
    [[nodiscard]] bool contains(const Key& key) const {
        std::shared_lock lock(m_mutex);
        return m_map.find(key) != m_map.end();
    }
    
    /**
     * @brief Put a value into the cache
     * 
     * If the key already exists, the value is updated and moved to front.
     * If the cache is full, the least recently used item is evicted.
     * 
     * @param key Key to store
     * @param value Value to store
     */
    void put(const Key& key, Value value) {
        std::unique_lock lock(m_mutex);
        
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            // Update existing entry
            it->second->second = std::move(value);
            m_list.splice(m_list.begin(), m_list, it->second);
            return;
        }
        
        // Evict if necessary
        while (m_list.size() >= m_maxSize && !m_list.empty()) {
            evictLast(lock);
        }
        
        // Insert new entry at front
        m_list.emplace_front(key, std::move(value));
        m_map[key] = m_list.begin();
    }
    
    /**
     * @brief Put a value using emplace semantics
     */
    template<typename... Args>
    void emplace(const Key& key, Args&&... args) {
        put(key, Value(std::forward<Args>(args)...));
    }
    
    /**
     * @brief Remove a specific key from the cache
     * 
     * @param key Key to remove
     * @return true if the key was found and removed
     */
    bool remove(const Key& key) {
        std::unique_lock lock(m_mutex);
        
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return false;
        }
        
        if (m_onEvict) {
            m_onEvict(it->second->first, it->second->second);
        }
        
        m_list.erase(it->second);
        m_map.erase(it);
        return true;
    }
    
    /**
     * @brief Clear all entries from the cache
     */
    void clear() {
        std::unique_lock lock(m_mutex);
        
        if (m_onEvict) {
            for (auto& item : m_list) {
                m_onEvict(item.first, item.second);
            }
        }
        
        m_list.clear();
        m_map.clear();
    }
    
    /**
     * @brief Get the current number of cached items
     */
    [[nodiscard]] size_t size() const {
        std::shared_lock lock(m_mutex);
        return m_list.size();
    }
    
    /**
     * @brief Check if the cache is empty
     */
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }
    
    /**
     * @brief Get the maximum capacity
     */
    [[nodiscard]] size_t capacity() const {
        return m_maxSize;
    }
    
    /**
     * @brief Resize the cache capacity
     * 
     * If the new size is smaller, excess items are evicted.
     */
    void resize(size_t newMaxSize) {
        std::unique_lock lock(m_mutex);
        
        m_maxSize = newMaxSize;
        
        while (m_list.size() > m_maxSize && !m_list.empty()) {
            evictLast(lock);
        }
    }
    
    /**
     * @brief Get cache statistics
     */
    struct Stats {
        size_t size;
        size_t capacity;
        uint64_t hits;
        uint64_t misses;
        
        [[nodiscard]] double hitRate() const {
            uint64_t total = hits + misses;
            return total > 0 ? static_cast<double>(hits) / total : 0.0;
        }
    };
    
    [[nodiscard]] Stats stats() const {
        std::shared_lock lock(m_mutex);
        return {m_list.size(), m_maxSize, m_hits, m_misses};
    }
    
    /**
     * @brief Get with stats tracking
     */
    [[nodiscard]] std::optional<Value> getTracked(const Key& key) {
        auto result = get(key);
        if (result) {
            ++m_hits;
        } else {
            ++m_misses;
        }
        return result;
    }
    
private:
    using ListType = std::list<std::pair<Key, Value>>;
    using MapType = std::unordered_map<Key, typename ListType::iterator>;
    
    void evictLast(std::unique_lock<std::shared_mutex>& /*lock*/) {
        if (m_list.empty()) return;
        
        auto& last = m_list.back();
        
        if (m_onEvict) {
            m_onEvict(last.first, last.second);
        }
        
        m_map.erase(last.first);
        m_list.pop_back();
    }
    
    size_t m_maxSize;
    ListType m_list;
    MapType m_map;
    EvictionCallback m_onEvict;
    
    mutable std::shared_mutex m_mutex;
    
    // Stats (atomic, no lock needed)
    mutable std::atomic<uint64_t> m_hits{0};
    mutable std::atomic<uint64_t> m_misses{0};
};

/**
 * @brief LRU cache with shared_ptr values for safe concurrent access
 * 
 * Values are stored as shared_ptr, allowing safe access to cached
 * items even after they might be evicted.
 */
template<typename Key, typename Value>
class SharedLRUCache {
public:
    using ValuePtr = std::shared_ptr<Value>;
    using EvictionCallback = std::function<void(const Key&, ValuePtr&)>;
    
    explicit SharedLRUCache(size_t maxSize, EvictionCallback onEvict = nullptr)
        : m_cache(maxSize, std::move(onEvict))
    {}
    
    [[nodiscard]] ValuePtr get(const Key& key) {
        auto result = m_cache.get(key);
        return result.value_or(nullptr);
    }
    
    void put(const Key& key, ValuePtr value) {
        m_cache.put(key, std::move(value));
    }
    
    template<typename... Args>
    ValuePtr emplace(const Key& key, Args&&... args) {
        auto ptr = std::make_shared<Value>(std::forward<Args>(args)...);
        m_cache.put(key, ptr);
        return ptr;
    }
    
    bool contains(const Key& key) const {
        return m_cache.contains(key);
    }
    
    bool remove(const Key& key) {
        return m_cache.remove(key);
    }
    
    void clear() {
        m_cache.clear();
    }
    
    [[nodiscard]] size_t size() const {
        return m_cache.size();
    }
    
    [[nodiscard]] size_t capacity() const {
        return m_cache.capacity();
    }
    
private:
    LRUCache<Key, ValuePtr> m_cache;
};

} // namespace phoenix
