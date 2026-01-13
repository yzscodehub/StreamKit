/**
 * @file uuid.cpp
 * @brief UUID implementation
 */

#include <phoenix/core/uuid.hpp>
#include <random>
#include <sstream>
#include <iomanip>

namespace phoenix {

UUID UUID::generate() {
    UUID uuid;
    
    // Use random device and mt19937 for generating random bytes
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dist;
    
    uint64_t high = dist(gen);
    uint64_t low = dist(gen);
    
    // Set version (4) and variant (RFC 4122)
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;  // Version 4
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;    // Variant RFC 4122
    
    // Store in big-endian order
    for (int i = 0; i < 8; ++i) {
        uuid.m_data[i] = static_cast<uint8_t>((high >> (56 - i * 8)) & 0xFF);
        uuid.m_data[8 + i] = static_cast<uint8_t>((low >> (56 - i * 8)) & 0xFF);
    }
    
    return uuid;
}

UUID UUID::fromString(const std::string& str) {
    UUID uuid;
    
    // Expected format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    if (str.length() != 36) {
        return uuid;  // Return null UUID on invalid input
    }
    
    std::string hex;
    for (char c : str) {
        if (c != '-') {
            hex += c;
        }
    }
    
    if (hex.length() != 32) {
        return uuid;
    }
    
    for (size_t i = 0; i < 16; ++i) {
        unsigned int byte;
        std::stringstream ss;
        ss << std::hex << hex.substr(i * 2, 2);
        ss >> byte;
        uuid.m_data[i] = static_cast<uint8_t>(byte);
    }
    
    return uuid;
}

std::string UUID::toString() const {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            ss << '-';
        }
        ss << std::setw(2) << static_cast<int>(m_data[i]);
    }
    
    return ss.str();
}

bool UUID::isNull() const {
    for (uint8_t byte : m_data) {
        if (byte != 0) return false;
    }
    return true;
}

} // namespace phoenix
