/**
 * @file result.hpp
 * @brief Error handling with Result<T, E> type
 * 
 * Provides a Rust-like Result type for explicit error handling
 * without exceptions in performance-critical paths.
 */

#pragma once

#include <variant>
#include <optional>
#include <type_traits>
#include <utility>
#include <stdexcept>

#include "types.hpp"

namespace phoenix {

// ============================================================================
// Error Type
// ============================================================================

/// Error class holding an error code and optional message
class Error {
public:
    Error() : code_(ErrorCode::Unknown) {}
    
    explicit Error(ErrorCode code) : code_(code) {}
    
    Error(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}
    
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }
    
    const char* what() const {
        if (!message_.empty()) {
            return message_.c_str();
        }
        return errorCodeToString(code_);
    }
    
    explicit operator bool() const { return code_ != ErrorCode::Ok; }
    
private:
    ErrorCode code_;
    std::string message_;
};

// ============================================================================
// Result<T, E> Template
// ============================================================================

/**
 * @brief Result type for functions that can fail
 * 
 * Similar to Rust's Result<T, E> or C++23's std::expected.
 * Holds either a success value (T) or an error (E).
 * 
 * @tparam T Success value type
 * @tparam E Error type (defaults to Error)
 */
template<typename T, typename E = Error>
class Result {
public:
    using value_type = T;
    using error_type = E;
    
    // ========== Constructors ==========
    
    /// Construct success result from value
    Result(T value) : data_(std::move(value)) {}
    
    /// Construct error result from error
    Result(E error) : data_(std::move(error)) {}
    
    /// Copy and move constructors
    Result(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    
    // ========== State Queries ==========
    
    /// Check if result is success
    [[nodiscard]] bool ok() const {
        return std::holds_alternative<T>(data_);
    }
    
    /// Check if result is error
    [[nodiscard]] bool isError() const {
        return std::holds_alternative<E>(data_);
    }
    
    /// Implicit bool conversion (true if success)
    explicit operator bool() const { return ok(); }
    
    // ========== Value Access ==========
    
    /// Get value reference (throws if error)
    T& value() & {
        if (!ok()) {
            throw std::runtime_error(std::get<E>(data_).what());
        }
        return std::get<T>(data_);
    }
    
    const T& value() const& {
        if (!ok()) {
            throw std::runtime_error(std::get<E>(data_).what());
        }
        return std::get<T>(data_);
    }
    
    T&& value() && {
        if (!ok()) {
            throw std::runtime_error(std::get<E>(data_).what());
        }
        return std::get<T>(std::move(data_));
    }
    
    /// Get value with default (no throw)
    T valueOr(T defaultValue) const& {
        if (ok()) {
            return std::get<T>(data_);
        }
        return defaultValue;
    }
    
    T valueOr(T defaultValue) && {
        if (ok()) {
            return std::get<T>(std::move(data_));
        }
        return defaultValue;
    }
    
    /// Get value pointer (nullptr if error)
    T* valuePtr() {
        if (ok()) {
            return &std::get<T>(data_);
        }
        return nullptr;
    }
    
    const T* valuePtr() const {
        if (ok()) {
            return &std::get<T>(data_);
        }
        return nullptr;
    }
    
    // ========== Error Access ==========
    
    /// Get error reference (throws if success)
    E& error() & {
        if (ok()) {
            throw std::logic_error("Result::error() called on success value");
        }
        return std::get<E>(data_);
    }
    
    const E& error() const& {
        if (ok()) {
            throw std::logic_error("Result::error() called on success value");
        }
        return std::get<E>(data_);
    }
    
    // ========== Monadic Operations ==========
    
    /// Map success value to new type
    template<typename F>
    auto map(F&& f) const -> Result<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (ok()) {
            return Result<U, E>(std::forward<F>(f)(std::get<T>(data_)));
        }
        return Result<U, E>(std::get<E>(data_));
    }
    
    /// Map error to new type
    template<typename F>
    auto mapError(F&& f) const -> Result<T, std::invoke_result_t<F, const E&>> {
        using E2 = std::invoke_result_t<F, const E&>;
        if (ok()) {
            return Result<T, E2>(std::get<T>(data_));
        }
        return Result<T, E2>(std::forward<F>(f)(std::get<E>(data_)));
    }
    
    /// Flat map (for chaining Result-returning functions)
    template<typename F>
    auto andThen(F&& f) const -> std::invoke_result_t<F, const T&> {
        using ResultType = std::invoke_result_t<F, const T&>;
        if (ok()) {
            return std::forward<F>(f)(std::get<T>(data_));
        }
        return ResultType(std::get<E>(data_));
    }
    
private:
    std::variant<T, E> data_;
};

// ============================================================================
// Result<void, E> Specialization
// ============================================================================

/**
 * @brief Specialization for void success type
 * 
 * Used for functions that either succeed with no value or fail.
 */
template<typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;
    
    /// Construct success result
    Result() : error_(std::nullopt) {}
    
    /// Construct error result
    Result(E error) : error_(std::move(error)) {}
    
    /// Copy and move
    Result(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    
    /// Check if success
    [[nodiscard]] bool ok() const { return !error_.has_value(); }
    
    /// Check if error
    [[nodiscard]] bool isError() const { return error_.has_value(); }
    
    /// Implicit bool conversion
    explicit operator bool() const { return ok(); }
    
    /// Get error (throws if success)
    E& error() & {
        if (!error_.has_value()) {
            throw std::logic_error("Result::error() called on success");
        }
        return *error_;
    }
    
    const E& error() const& {
        if (!error_.has_value()) {
            throw std::logic_error("Result::error() called on success");
        }
        return *error_;
    }
    
private:
    std::optional<E> error_;
};

// ============================================================================
// Helper Factory Functions
// ============================================================================

/// Create success result
template<typename T>
inline Result<T> Ok(T value) {
    return Result<T>(std::move(value));
}

/// Create void success result
inline Result<void> Ok() {
    return Result<void>();
}

/// Create error result from code
template<typename T = void>
inline Result<T> Err(ErrorCode code) {
    return Result<T>(Error(code));
}

/// Create error result from code and message
template<typename T = void>
inline Result<T> Err(ErrorCode code, std::string message) {
    return Result<T>(Error(code, std::move(message)));
}

/// Create error result from Error object
template<typename T = void>
inline Result<T> Err(Error error) {
    return Result<T>(std::move(error));
}

} // namespace phoenix

