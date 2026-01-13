/**
 * @file decoder_pool.hpp
 * @brief Decoder pool for efficient decoder reuse
 * 
 * DecoderPool manages a pool of Decoder instances, allowing
 * efficient reuse of decoders for the same media files.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/decoder.hpp>
#include <phoenix/media/frame.hpp>
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>
#include <filesystem>

namespace phoenix::media {

/**
 * @brief Decoder pool configuration
 */
struct DecoderPoolConfig {
    size_t maxDecoders = 8;          // Maximum number of pooled decoders
    size_t maxPerFile = 2;           // Maximum decoders per unique file
    Duration idleTimeoutMs = 30000;  // Close idle decoders after 30s
    CodecPreference codecPreference = CodecPreference::Auto;
};

/**
 * @brief Pooled decoder handle
 * 
 * RAII wrapper that returns the decoder to the pool when destroyed.
 */
class PooledDecoder {
public:
    PooledDecoder() = default;
    ~PooledDecoder();
    
    // Move only
    PooledDecoder(PooledDecoder&& other) noexcept;
    PooledDecoder& operator=(PooledDecoder&& other) noexcept;
    PooledDecoder(const PooledDecoder&) = delete;
    PooledDecoder& operator=(const PooledDecoder&) = delete;
    
    /// Get the underlying decoder
    Decoder* get() { return m_decoder.get(); }
    const Decoder* get() const { return m_decoder.get(); }
    
    Decoder* operator->() { return m_decoder.get(); }
    const Decoder* operator->() const { return m_decoder.get(); }
    
    Decoder& operator*() { return *m_decoder; }
    const Decoder& operator*() const { return *m_decoder; }
    
    explicit operator bool() const { return m_decoder != nullptr; }
    
private:
    friend class DecoderPool;
    
    PooledDecoder(std::unique_ptr<Decoder> decoder, class DecoderPool* pool,
                  const std::filesystem::path& path);
    
    std::unique_ptr<Decoder> m_decoder;
    DecoderPool* m_pool = nullptr;
    std::filesystem::path m_path;
};

/**
 * @brief Decoder pool for efficient decoder management
 * 
 * Maintains a pool of decoder instances that can be reused.
 * Decoders are keyed by file path and returned to the pool
 * when no longer needed.
 * 
 * Thread-safe: All methods can be called from any thread.
 */
class DecoderPool {
public:
    explicit DecoderPool(const DecoderPoolConfig& config = {});
    ~DecoderPool();
    
    // Non-copyable
    DecoderPool(const DecoderPool&) = delete;
    DecoderPool& operator=(const DecoderPool&) = delete;
    
    /**
     * @brief Acquire a decoder for a file
     * 
     * If a pooled decoder exists for the file, it is returned.
     * Otherwise, a new decoder is created.
     * 
     * @param path Path to media file
     * @return Pooled decoder or error
     */
    Result<PooledDecoder, Error> acquire(const std::filesystem::path& path);
    
    /**
     * @brief Decode a single frame (convenience method)
     * 
     * Acquires a decoder, decodes the frame, and returns the decoder
     * to the pool.
     * 
     * @param path Path to media file
     * @param time Target time
     * @return Decoded frame or error
     */
    Result<VideoFrame, Error> decodeFrame(
        const std::filesystem::path& path, Timestamp time);
    
    /**
     * @brief Clear all pooled decoders
     */
    void clear();
    
    /**
     * @brief Clear idle decoders
     * 
     * Removes decoders that haven't been used recently.
     */
    void clearIdle();
    
    /**
     * @brief Get pool statistics
     */
    struct Stats {
        size_t pooledDecoders;    // Decoders currently in pool
        size_t activeDecoders;    // Decoders currently in use
        size_t totalCreated;      // Total decoders ever created
        size_t cacheHits;         // Times a pooled decoder was reused
        size_t cacheMisses;       // Times a new decoder was created
    };
    
    [[nodiscard]] Stats stats() const;
    
    /**
     * @brief Get configuration
     */
    [[nodiscard]] const DecoderPoolConfig& config() const { return m_config; }
    
private:
    friend class PooledDecoder;
    
    /// Return a decoder to the pool
    void release(std::unique_ptr<Decoder> decoder, 
                 const std::filesystem::path& path);
    
    struct PoolEntry {
        std::unique_ptr<Decoder> decoder;
        std::chrono::steady_clock::time_point lastUsed;
    };
    
    DecoderPoolConfig m_config;
    
    // Pool storage: path -> list of available decoders
    std::unordered_map<std::string, std::list<PoolEntry>> m_pool;
    
    mutable std::mutex m_mutex;
    
    // Statistics
    size_t m_activeCount = 0;
    size_t m_totalCreated = 0;
    size_t m_cacheHits = 0;
    size_t m_cacheMisses = 0;
};

} // namespace phoenix::media
