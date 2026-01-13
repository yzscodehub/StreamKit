/**
 * @file decoder_pool.cpp
 * @brief DecoderPool implementation
 */

#include <phoenix/media/decoder_pool.hpp>

namespace phoenix::media {

// ============================================================================
// PooledDecoder
// ============================================================================

PooledDecoder::PooledDecoder(std::unique_ptr<Decoder> decoder, 
                             DecoderPool* pool,
                             const std::filesystem::path& path)
    : m_decoder(std::move(decoder))
    , m_pool(pool)
    , m_path(path)
{}

PooledDecoder::~PooledDecoder() {
    if (m_decoder && m_pool) {
        m_pool->release(std::move(m_decoder), m_path);
    }
}

PooledDecoder::PooledDecoder(PooledDecoder&& other) noexcept
    : m_decoder(std::move(other.m_decoder))
    , m_pool(other.m_pool)
    , m_path(std::move(other.m_path))
{
    other.m_pool = nullptr;
}

PooledDecoder& PooledDecoder::operator=(PooledDecoder&& other) noexcept {
    if (this != &other) {
        // Release current decoder first
        if (m_decoder && m_pool) {
            m_pool->release(std::move(m_decoder), m_path);
        }
        
        m_decoder = std::move(other.m_decoder);
        m_pool = other.m_pool;
        m_path = std::move(other.m_path);
        other.m_pool = nullptr;
    }
    return *this;
}

// ============================================================================
// DecoderPool
// ============================================================================

DecoderPool::DecoderPool(const DecoderPoolConfig& config)
    : m_config(config)
{}

DecoderPool::~DecoderPool() {
    clear();
}

Result<PooledDecoder, Error> DecoderPool::acquire(
    const std::filesystem::path& path)
{
    std::string pathKey = path.string();
    
    std::lock_guard lock(m_mutex);
    
    // Try to get from pool
    auto it = m_pool.find(pathKey);
    if (it != m_pool.end() && !it->second.empty()) {
        // Take the most recently used decoder
        auto& list = it->second;
        auto decoder = std::move(list.back().decoder);
        list.pop_back();
        
        if (list.empty()) {
            m_pool.erase(it);
        }
        
        ++m_cacheHits;
        ++m_activeCount;
        
        // Seek to start for clean state
        decoder->seekToStart();
        
        return PooledDecoder(std::move(decoder), this, path);
    }
    
    // Create new decoder
    ++m_cacheMisses;
    ++m_totalCreated;
    ++m_activeCount;
    
    auto decoder = std::make_unique<Decoder>();
    
    DecoderConfig config;
    config.path = path;
    config.codecPreference = m_config.codecPreference;
    
    auto result = decoder->open(config);
    if (!result.ok()) {
        --m_activeCount;
        return result.error();
    }
    
    return PooledDecoder(std::move(decoder), this, path);
}

void DecoderPool::release(std::unique_ptr<Decoder> decoder,
                          const std::filesystem::path& path)
{
    if (!decoder) return;
    
    std::string pathKey = path.string();
    
    std::lock_guard lock(m_mutex);
    
    --m_activeCount;
    
    // Check if we should pool this decoder
    auto& list = m_pool[pathKey];
    
    // Check per-file limit
    if (list.size() >= m_config.maxPerFile) {
        // Don't pool, let it be destroyed
        return;
    }
    
    // Check total limit
    size_t totalPooled = 0;
    for (const auto& [key, entries] : m_pool) {
        totalPooled += entries.size();
    }
    
    if (totalPooled >= m_config.maxDecoders) {
        // Find and remove oldest entry
        auto oldestIt = m_pool.end();
        auto oldestTime = std::chrono::steady_clock::time_point::max();
        
        for (auto it = m_pool.begin(); it != m_pool.end(); ++it) {
            if (!it->second.empty()) {
                auto& front = it->second.front();
                if (front.lastUsed < oldestTime) {
                    oldestTime = front.lastUsed;
                    oldestIt = it;
                }
            }
        }
        
        if (oldestIt != m_pool.end()) {
            oldestIt->second.pop_front();
            if (oldestIt->second.empty()) {
                m_pool.erase(oldestIt);
            }
        }
    }
    
    // Add to pool
    PoolEntry entry;
    entry.decoder = std::move(decoder);
    entry.lastUsed = std::chrono::steady_clock::now();
    list.push_back(std::move(entry));
}

Result<VideoFrame, Error> DecoderPool::decodeFrame(
    const std::filesystem::path& path, Timestamp time)
{
    auto decoderResult = acquire(path);
    if (!decoderResult.ok()) {
        return decoderResult.error();
    }
    
    auto& decoder = decoderResult.value();
    return decoder->decodeVideoFrame(time);
}

void DecoderPool::clear() {
    std::lock_guard lock(m_mutex);
    m_pool.clear();
}

void DecoderPool::clearIdle() {
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(m_config.idleTimeoutMs);
    
    std::lock_guard lock(m_mutex);
    
    for (auto it = m_pool.begin(); it != m_pool.end(); ) {
        auto& list = it->second;
        
        // Remove old entries
        list.remove_if([&](const PoolEntry& entry) {
            return (now - entry.lastUsed) > timeout;
        });
        
        if (list.empty()) {
            it = m_pool.erase(it);
        } else {
            ++it;
        }
    }
}

DecoderPool::Stats DecoderPool::stats() const {
    std::lock_guard lock(m_mutex);
    
    size_t pooled = 0;
    for (const auto& [key, list] : m_pool) {
        pooled += list.size();
    }
    
    return {
        pooled,
        m_activeCount,
        m_totalCreated,
        m_cacheHits,
        m_cacheMisses
    };
}

} // namespace phoenix::media
