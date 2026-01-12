/**
 * @file concepts.hpp
 * @brief C++20 Concepts for the Flow Graph system
 * 
 * Defines type constraints for data types that can flow through
 * the pipeline. All transferable types must be movable.
 */

#pragma once

#include <concepts>
#include <type_traits>

namespace phoenix {

/**
 * @brief Concept for types that can be transferred through the pipeline
 * 
 * Requirements:
 * - Must be move constructible
 * - Must be move assignable
 * - Should be destructible
 */
template<typename T>
concept Transferable = 
    std::move_constructible<T> && 
    std::movable<T> &&
    std::destructible<T>;

/**
 * @brief Concept for types that have an isEof() method
 */
template<typename T>
concept HasEof = requires(const T& t) {
    { t.isEof() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for types that have a serial number
 */
template<typename T>
concept HasSerial = requires(const T& t) {
    { t.serial } -> std::convertible_to<uint64_t>;
};

/**
 * @brief Concept for types that have a pts (presentation timestamp)
 */
template<typename T>
concept HasPts = requires(const T& t) {
    { t.pts } -> std::convertible_to<int64_t>;
};

/**
 * @brief Concept for node types that can be started/stopped
 */
template<typename T>
concept Startable = requires(T& t) {
    { t.start() } -> std::same_as<void>;
    { t.stop() } -> std::same_as<void>;
};

/**
 * @brief Concept for node types that can be flushed (for seek)
 */
template<typename T>
concept Flushable = requires(T& t) {
    { t.flush() } -> std::same_as<void>;
};

/**
 * @brief Concept for types that can process input and produce output
 */
template<typename T, typename TIn, typename TOut>
concept Processor = requires(T& t, TIn input) {
    { t.process(std::move(input)) } -> std::same_as<void>;
};

/**
 * @brief Concept for source types that produce data
 */
template<typename T, typename TOut>
concept Source = requires(T& t) {
    requires Startable<T>;
};

/**
 * @brief Concept for sink types that consume data
 */
template<typename T, typename TIn>
concept Sink = requires(T& t, TIn input) {
    { t.consume(std::move(input)) } -> std::same_as<void>;
};

} // namespace phoenix

