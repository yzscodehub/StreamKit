/**
 * @file frame_cache.hpp
 * @brief Frame caching system for decoded video frames
 * 
 * Provides an LRU cache for decoded video frames with
 * timeline-aware lookup and prefetching support.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/lru_cache.hpp>
#include <phoenix/media/frame.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <string>

namespace phoenix::engine {

/**
 * @brief Key for frame cache lookup
 */
struct FrameCacheKey {
    UUID clipId;           ///< Which clip this frame belongs to
    Timestamp mediaTime;   ///< Time within the media file
    
    bool operator==(const FrameCacheKey& other) const {
        return clipId == other.clipId && mediaTime == other.mediaTime;
    }
};

} // namespace phoenix::engine

// Hash specialization for FrameCacheKey
template<>
struct std::hash<phoenix::engine::FrameCacheKey> {
    size_t operator()(const phoenix::engine::FrameCacheKey& key) const noexcept {
        size_t h1 = std::hash<phoenix::UUID>{}(key.clipId);
        size_t h2 = std::hash<phoenix::Timestamp>{}(key.mediaTime);
        return h1 ^ (h2 << 1);
    }
};

namespace phoenix::engine {

/**
 * @brief Cached frame entry
 */
struct CachedFrame {
    std::shared_ptr<media::VideoFrame> frame;
    uint64_t accessCount = 0;
    
    CachedFrame() = default;
    explicit CachedFrame(std::shared_ptr<media::VideoFrame> f)
        : frame(std::move(f)) {}
};

/**
 * @brief Frame cache statistics
 */
struct FrameCacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    size_t currentSize = 0;
    size_t maxSize = 0;
    size_t memoryUsage = 0;
    
    [[nodiscard]] double hitRate() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
};

/**
 * @brief Frame cache with LRU eviction
 * 
 * Caches decoded video frames for quick access during
 * playback and scrubbing. Uses timeline positions for
 * efficient lookup.
 * 
 * Thread-safe for concurrent access.
 */
class FrameCache {
public:
    /**
     * @brief Construct cache with given capacity
     * 
     * @param maxFrames Maximum number of frames to cache
     * @param maxMemoryMB Maximum memory usage in megabytes
     */
    explicit FrameCache(size_t maxFrames = 100, size_t maxMemoryMB = 512)
        : m_maxFrames(maxFrames)
        , m_maxMemory(maxMemoryMB * 1024 * 1024) {}
    
    // ========== Cache Operations ==========
    
    /**
     * @brief Get a frame from cache
     * 
     * @param clipId Clip identifier
     * @param mediaTime Time within the media
     * @return Cached frame or nullptr if not found
     */
    std::shared_ptr<media::VideoFrame> get(const UUID& clipId, Timestamp mediaTime) {
        std::lock_guard lock(m_mutex);
        
        FrameCacheKey key{clipId, mediaTime};
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_stats.hits++;
            it->second.accessCount++;
            
            // Move to front of LRU list
            moveToFront(key);
            
            return it->second.frame;
        }
        
        m_stats.misses++;
        return nullptr;
    }
    
    /**
     * @brief Store a frame in cache
     * 
     * @param clipId Clip identifier
     * @param mediaTime Time within the media
     * @param frame Frame to cache
     */
    void put(const UUID& clipId, Timestamp mediaTime, 
             std::shared_ptr<media::VideoFrame> frame) {
        if (!frame) return;
        
        std::lock_guard lock(m_mutex);
        
        FrameCacheKey key{clipId, mediaTime};
        
        // Check if already exists
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            // Update existing
            m_memoryUsage -= estimateFrameSize(*it->second.frame);
            it->second.frame = frame;
            m_memoryUsage += estimateFrameSize(*frame);
            moveToFront(key);
            return;
        }
        
        // Calculate frame size
        size_t frameSize = estimateFrameSize(*frame);
        
        // Evict if necessary
        while (m_cache.size() >= m_maxFrames || 
               (m_maxMemory > 0 && m_memoryUsage + frameSize > m_maxMemory)) {
            if (!evictOne()) break;
        }
        
        // Insert new frame
        m_cache.emplace(key, CachedFrame{frame});
        m_lruList.push_front(key);
        m_lruMap[key] = m_lruList.begin();
        m_memoryUsage += frameSize;
        
        m_stats.currentSize = m_cache.size();
        m_stats.maxSize = std::max(m_stats.maxSize, m_cache.size());
        m_stats.memoryUsage = m_memoryUsage;
    }
    
    /**
     * @brief Check if frame is in cache
     */
    bool contains(const UUID& clipId, Timestamp mediaTime) const {
        std::lock_guard lock(m_mutex);
        return m_cache.find({clipId, mediaTime}) != m_cache.end();
    }
    
    /**
     * @brief Remove specific frame from cache
     */
    void remove(const UUID& clipId, Timestamp mediaTime) {
        std::lock_guard lock(m_mutex);
        
        FrameCacheKey key{clipId, mediaTime};
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_memoryUsage -= estimateFrameSize(*it->second.frame);
            m_cache.erase(it);
            
            auto lruIt = m_lruMap.find(key);
            if (lruIt != m_lruMap.end()) {
                m_lruList.erase(lruIt->second);
                m_lruMap.erase(lruIt);
            }
        }
    }
    
    /**
     * @brief Remove all frames for a clip
     */
    void removeClip(const UUID& clipId) {
        std::lock_guard lock(m_mutex);
        
        for (auto it = m_cache.begin(); it != m_cache.end(); ) {
            if (it->first.clipId == clipId) {
                m_memoryUsage -= estimateFrameSize(*it->second.frame);
                
                auto lruIt = m_lruMap.find(it->first);
                if (lruIt != m_lruMap.end()) {
                    m_lruList.erase(lruIt->second);
                    m_lruMap.erase(lruIt);
                }
                
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    /**
     * @brief Clear entire cache
     */
    void clear() {
        std::lock_guard lock(m_mutex);
        m_cache.clear();
        m_lruList.clear();
        m_lruMap.clear();
        m_memoryUsage = 0;
    }
    
    // ========== Prefetching ==========
    
    /**
     * @brief Get range of frames to prefetch
     * 
     * Returns frame times that should be decoded and cached
     * for smooth playback around the given time.
     * 
     * @param clipId Clip to prefetch
     * @param currentTime Current playback time
     * @param frameDuration Duration of one frame
     * @param count Number of frames to prefetch
     * @return List of media times to prefetch
     */
    std::vector<Timestamp> getPrefetchRange(
            const UUID& clipId,
            Timestamp currentTime,
            Duration frameDuration,
            size_t count = 10) const {
        std::lock_guard lock(m_mutex);
        
        std::vector<Timestamp> result;
        result.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            Timestamp time = currentTime + frameDuration * static_cast<int64_t>(i);
            FrameCacheKey key{clipId, time};
            if (m_cache.find(key) == m_cache.end()) {
                result.push_back(time);
            }
        }
        
        return result;
    }
    
    // ========== Statistics ==========
    
    [[nodiscard]] FrameCacheStats stats() const {
        std::lock_guard lock(m_mutex);
        return m_stats;
    }
    
    [[nodiscard]] size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_cache.size();
    }
    
    [[nodiscard]] size_t memoryUsage() const {
        std::lock_guard lock(m_mutex);
        return m_memoryUsage;
    }
    
private:
    /**
     * @brief Estimate memory size of a frame
     */
    static size_t estimateFrameSize(const media::VideoFrame& frame) {
        // Rough estimate: width * height * bytes per pixel
        // Assume RGBA or similar format
        return static_cast<size_t>(frame.width()) * frame.height() * 4;
    }
    
    /**
     * @brief Move key to front of LRU list
     */
    void moveToFront(const FrameCacheKey& key) {
        auto it = m_lruMap.find(key);
        if (it != m_lruMap.end()) {
            m_lruList.erase(it->second);
            m_lruList.push_front(key);
            it->second = m_lruList.begin();
        }
    }
    
    /**
     * @brief Evict least recently used frame
     * @return true if a frame was evicted
     */
    bool evictOne() {
        if (m_lruList.empty()) return false;
        
        auto key = m_lruList.back();
        m_lruList.pop_back();
        m_lruMap.erase(key);
        
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_memoryUsage -= estimateFrameSize(*it->second.frame);
            m_cache.erase(it);
        }
        
        return true;
    }
    
private:
    mutable std::mutex m_mutex;
    
    size_t m_maxFrames;
    size_t m_maxMemory;
    size_t m_memoryUsage = 0;
    
    std::unordered_map<FrameCacheKey, CachedFrame> m_cache;
    
    // LRU tracking
    std::list<FrameCacheKey> m_lruList;
    std::unordered_map<FrameCacheKey, std::list<FrameCacheKey>::iterator> m_lruMap;
    
    mutable FrameCacheStats m_stats;
};

} // namespace phoenix::engine
