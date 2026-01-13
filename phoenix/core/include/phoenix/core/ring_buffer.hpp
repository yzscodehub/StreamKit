/**
 * @file ring_buffer.hpp
 * @brief Lock-free ring buffer for audio playback
 * 
 * Key design:
 * - Single producer (audio decoder), single consumer (SDL callback)
 * - Lock-free using atomic head/tail pointers
 * - Power-of-2 size for efficient modulo via bitmask
 * - Sized for 400-600ms of audio data at 48kHz stereo 16-bit
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>
#include <thread>

namespace phoenix {

/**
 * @brief Lock-free single-producer single-consumer ring buffer
 * 
 * Thread safety:
 * - write(): Single producer thread (audio decode/resample)
 * - read(): Single consumer thread (SDL audio callback)
 * - clear(): Can be called from producer when synchronized
 * - availableRead/Write(): Safe from any thread
 */
class LockFreeRingBuffer {
public:
    /// Default capacity: 128KB (~680ms at 48kHz/stereo/16-bit)
    static constexpr size_t kDefaultCapacity = 131072;
    
    /// Minimum capacity: 16KB
    static constexpr size_t kMinCapacity = 16384;
    
    /// Maximum capacity: 1MB
    static constexpr size_t kMaxCapacity = 1048576;
    
    /**
     * @brief Construct ring buffer with given capacity
     * @param capacity Buffer size in bytes (will be rounded to power of 2)
     */
    explicit LockFreeRingBuffer(size_t capacity = kDefaultCapacity)
        : m_mask(roundToPowerOf2(std::clamp(capacity, kMinCapacity, kMaxCapacity)) - 1)
        , m_buffer(m_mask + 1)
    {}
    
    // Non-copyable, non-movable (due to atomic members)
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;
    
    // ========== Producer Interface ==========
    
    /**
     * @brief Write data to the buffer
     * 
     * Non-blocking: writes as much as possible, returns actual bytes written.
     * 
     * @param data Source data pointer
     * @param size Number of bytes to write
     * @return Actual bytes written (may be less than size if buffer full)
     */
    size_t write(const uint8_t* data, size_t size) {
        if (size == 0 || !data) return 0;
        
        const size_t writePos = m_writePos.load(std::memory_order_relaxed);
        const size_t readPos = m_readPos.load(std::memory_order_acquire);
        
        // Calculate available space
        const size_t available = capacity() - (writePos - readPos);
        const size_t toWrite = std::min(size, available);
        
        if (toWrite == 0) return 0;
        
        // Write in one or two chunks (handle wrap-around)
        const size_t writeIdx = writePos & m_mask;
        const size_t firstChunk = std::min(toWrite, capacity() - writeIdx);
        const size_t secondChunk = toWrite - firstChunk;
        
        std::memcpy(m_buffer.data() + writeIdx, data, firstChunk);
        if (secondChunk > 0) {
            std::memcpy(m_buffer.data(), data + firstChunk, secondChunk);
        }
        
        // Publish write
        m_writePos.store(writePos + toWrite, std::memory_order_release);
        
        return toWrite;
    }
    
    /**
     * @brief Write all data, blocking until space available
     */
    bool writeAll(const uint8_t* data, size_t size) {
        size_t written = 0;
        while (written < size) {
            size_t n = write(data + written, size - written);
            if (n == 0) {
                std::this_thread::yield();
            }
            written += n;
        }
        return true;
    }
    
    /**
     * @brief Get available write space
     */
    [[nodiscard]] size_t availableWrite() const {
        const size_t writePos = m_writePos.load(std::memory_order_relaxed);
        const size_t readPos = m_readPos.load(std::memory_order_acquire);
        return capacity() - (writePos - readPos);
    }
    
    // ========== Consumer Interface ==========
    
    /**
     * @brief Read data from the buffer
     * 
     * Non-blocking: reads as much as available, returns actual bytes read.
     * 
     * @param dest Destination buffer
     * @param size Maximum bytes to read
     * @return Actual bytes read (may be less than size if buffer empty)
     */
    size_t read(uint8_t* dest, size_t size) {
        if (size == 0 || !dest) return 0;
        
        const size_t readPos = m_readPos.load(std::memory_order_relaxed);
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        
        // Calculate available data
        const size_t available = writePos - readPos;
        const size_t toRead = std::min(size, available);
        
        if (toRead == 0) return 0;
        
        // Read in one or two chunks (handle wrap-around)
        const size_t readIdx = readPos & m_mask;
        const size_t firstChunk = std::min(toRead, capacity() - readIdx);
        const size_t secondChunk = toRead - firstChunk;
        
        std::memcpy(dest, m_buffer.data() + readIdx, firstChunk);
        if (secondChunk > 0) {
            std::memcpy(dest + firstChunk, m_buffer.data(), secondChunk);
        }
        
        // Publish read
        m_readPos.store(readPos + toRead, std::memory_order_release);
        
        return toRead;
    }
    
    /**
     * @brief Peek at data without consuming
     */
    [[nodiscard]] size_t peek(uint8_t* dest, size_t size) const {
        if (size == 0 || !dest) return 0;
        
        const size_t readPos = m_readPos.load(std::memory_order_relaxed);
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        
        const size_t available = writePos - readPos;
        const size_t toPeek = std::min(size, available);
        
        if (toPeek == 0) return 0;
        
        const size_t readIdx = readPos & m_mask;
        const size_t firstChunk = std::min(toPeek, capacity() - readIdx);
        const size_t secondChunk = toPeek - firstChunk;
        
        std::memcpy(dest, m_buffer.data() + readIdx, firstChunk);
        if (secondChunk > 0) {
            std::memcpy(dest + firstChunk, m_buffer.data(), secondChunk);
        }
        
        return toPeek;
    }
    
    /**
     * @brief Skip (consume) data without reading
     */
    size_t skip(size_t size) {
        const size_t readPos = m_readPos.load(std::memory_order_relaxed);
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        
        const size_t available = writePos - readPos;
        const size_t toSkip = std::min(size, available);
        
        if (toSkip > 0) {
            m_readPos.store(readPos + toSkip, std::memory_order_release);
        }
        
        return toSkip;
    }
    
    /**
     * @brief Get available read data
     */
    [[nodiscard]] size_t availableRead() const {
        const size_t readPos = m_readPos.load(std::memory_order_relaxed);
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        return writePos - readPos;
    }
    
    // ========== Buffer Management ==========
    
    /**
     * @brief Clear the buffer
     */
    void clear() {
        m_readPos.store(0, std::memory_order_release);
        m_writePos.store(0, std::memory_order_release);
    }
    
    /**
     * @brief Get buffer capacity
     */
    [[nodiscard]] size_t capacity() const {
        return m_mask + 1;
    }
    
    /**
     * @brief Check if buffer is empty
     */
    [[nodiscard]] bool empty() const {
        return availableRead() == 0;
    }
    
    /**
     * @brief Check if buffer is full
     */
    [[nodiscard]] bool full() const {
        return availableWrite() == 0;
    }
    
    /**
     * @brief Get fill ratio (0.0 - 1.0)
     */
    [[nodiscard]] float fillRatio() const {
        return static_cast<float>(availableRead()) / static_cast<float>(capacity());
    }
    
private:
    /**
     * @brief Round up to next power of 2
     */
    [[nodiscard]] static size_t roundToPowerOf2(size_t n) {
        if (n == 0) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }
    
    /// Bitmask for efficient modulo (capacity - 1)
    const size_t m_mask;
    
    /// Data buffer
    std::vector<uint8_t> m_buffer;
    
    /// Write position (only modified by producer)
    alignas(64) std::atomic<size_t> m_writePos{0};
    
    /// Read position (only modified by consumer)
    alignas(64) std::atomic<size_t> m_readPos{0};
};

/**
 * @brief Calculate required buffer size for given audio parameters
 */
[[nodiscard]] inline size_t calculateAudioBufferSize(
    int sampleRate, 
    int channels, 
    int bytesPerSample, 
    int durationMs
) {
    return static_cast<size_t>(sampleRate) * channels * bytesPerSample * durationMs / 1000;
}

} // namespace phoenix
