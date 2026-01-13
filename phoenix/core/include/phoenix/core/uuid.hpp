/**
 * @file uuid.hpp
 * @brief UUID generation for unique identification
 * 
 * Provides a simple UUID type for identifying project entities
 * (clips, tracks, sequences, etc.)
 */

#pragma once

#include <string>
#include <cstdint>
#include <array>
#include <functional>

namespace phoenix {

/**
 * @brief UUID (Universally Unique Identifier)
 * 
 * Uses a simple 128-bit random UUID (v4).
 */
class UUID {
public:
    /// Create a null (empty) UUID
    UUID() : m_data{} {}
    
    /// Generate a new random UUID
    static UUID generate();
    
    /// Create UUID from string (e.g., "550e8400-e29b-41d4-a716-446655440000")
    static UUID fromString(const std::string& str);
    
    /// Convert to string representation
    std::string toString() const;
    
    /// Check if UUID is null (all zeros)
    bool isNull() const;
    
    /// Comparison operators
    bool operator==(const UUID& other) const {
        return m_data == other.m_data;
    }
    
    bool operator!=(const UUID& other) const {
        return m_data != other.m_data;
    }
    
    bool operator<(const UUID& other) const {
        return m_data < other.m_data;
    }
    
    /// Get raw data (for serialization)
    const std::array<uint8_t, 16>& data() const { return m_data; }
    
    /// Set raw data (for deserialization)
    void setData(const std::array<uint8_t, 16>& data) { m_data = data; }
    
private:
    std::array<uint8_t, 16> m_data;
};

} // namespace phoenix

// Hash support for std::unordered_map
namespace std {
template<>
struct hash<phoenix::UUID> {
    size_t operator()(const phoenix::UUID& uuid) const {
        const auto& data = uuid.data();
        size_t h = 0;
        for (size_t i = 0; i < 16; ++i) {
            h ^= std::hash<uint8_t>{}(data[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};
} // namespace std
