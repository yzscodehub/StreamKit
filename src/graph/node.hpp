/**
 * @file node.hpp
 * @brief Base classes for Flow Graph nodes
 * 
 * Node hierarchy:
 * - INode: Base interface with start/stop/flush
 * - SourceNode<T>: Produces data (e.g., FileSourceNode)
 * - ProcessorNode<TIn, TOut>: Transforms data (e.g., DecoderNode)
 * - SinkNode<T>: Consumes data (e.g., VideoSinkNode)
 * 
 * Key design:
 * - Nodes don't manage threads internally
 * - AsyncQueueNode provides thread boundaries
 * - process() returns void and can emit 0..N outputs
 */

#pragma once

#include <string>
#include <atomic>
#include <memory>

#include "concepts.hpp"
#include "pin.hpp"
#include "core/types.hpp"
#include "core/result.hpp"

namespace phoenix {

// ============================================================================
// INode - Base interface
// ============================================================================

/**
 * @brief Base interface for all graph nodes
 * 
 * All nodes must implement start(), stop(), and flush().
 */
class INode {
public:
    virtual ~INode() = default;
    
    /**
     * @brief Get node name (for debugging/logging)
     */
    [[nodiscard]] virtual const std::string& name() const = 0;
    
    /**
     * @brief Start the node
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop the node
     */
    virtual void stop() = 0;
    
    /**
     * @brief Flush internal state (for seek operations)
     */
    virtual void flush() {}
    
    /**
     * @brief Check if node is running
     */
    [[nodiscard]] virtual bool isRunning() const = 0;
};

// ============================================================================
// NodeBase - Common implementation
// ============================================================================

/**
 * @brief Base class with common node functionality
 */
class NodeBase : public INode {
public:
    explicit NodeBase(std::string name) : name_(std::move(name)) {}
    
    [[nodiscard]] const std::string& name() const override { return name_; }
    
    [[nodiscard]] bool isRunning() const override {
        return running_.load(std::memory_order_acquire);
    }
    
protected:
    std::string name_;
    std::atomic<bool> running_{false};
};

// ============================================================================
// SourceNode - Produces data
// ============================================================================

/**
 * @brief Node that produces data without input
 * 
 * Examples: FileSourceNode, NetworkSourceNode
 * 
 * @tparam TOut Output data type
 */
template<Transferable TOut>
class SourceNode : public NodeBase {
public:
    using OutputType = TOut;
    
    explicit SourceNode(std::string name, size_t outputCapacity = kDefaultVideoQueueCapacity)
        : NodeBase(std::move(name))
    {}
    
    /// Output pin for produced data
    OutputPin<TOut> output;
    
    void start() override {
        running_.store(true, std::memory_order_release);
    }
    
    void stop() override {
        running_.store(false, std::memory_order_release);
    }
    
protected:
    /**
     * @brief Emit data to output
     */
    Result<void> emit(TOut data) {
        return output.emit(std::move(data));
    }
    
    /**
     * @brief Try to emit without blocking
     */
    bool tryEmit(TOut data) {
        return output.tryEmit(std::move(data));
    }
};

// ============================================================================
// ProcessorNode - Transforms data (1-to-N)
// ============================================================================

/**
 * @brief Node that processes input and produces output
 * 
 * CRITICAL: process() returns void and may emit 0, 1, or N outputs.
 * This is essential for decoder nodes (B-frame buffering).
 * 
 * Examples: FFmpegDecodeNode, FilterNode
 * 
 * @tparam TIn Input data type
 * @tparam TOut Output data type
 */
template<Transferable TIn, Transferable TOut>
class ProcessorNode : public NodeBase {
public:
    using InputType = TIn;
    using OutputType = TOut;
    
    explicit ProcessorNode(std::string name, 
                          size_t inputCapacity = kDefaultVideoQueueCapacity)
        : NodeBase(std::move(name))
        , input(inputCapacity)
    {}
    
    /// Input pin for receiving data
    InputPin<TIn> input;
    
    /// Output pin for produced data
    OutputPin<TOut> output;
    
    void start() override {
        running_.store(true, std::memory_order_release);
    }
    
    void stop() override {
        running_.store(false, std::memory_order_release);
        input.stop();  // Wake any blocked consumers
    }
    
    void flush() override {
        input.flush();
    }
    
    /**
     * @brief Process one input item
     * 
     * Implementation should call emit() 0..N times.
     * Must handle EOF input specially.
     * 
     * @param data Input data to process
     */
    virtual void process(TIn data) = 0;
    
protected:
    /**
     * @brief Emit output data
     */
    Result<void> emit(TOut data) {
        return output.emit(std::move(data));
    }
    
    /**
     * @brief Try to emit without blocking
     */
    bool tryEmit(TOut data) {
        return output.tryEmit(std::move(data));
    }
};

// ============================================================================
// SinkNode - Consumes data
// ============================================================================

/**
 * @brief Node that consumes data without producing output
 * 
 * Examples: VideoSinkNode, AudioSinkNode
 * 
 * @tparam TIn Input data type
 */
template<Transferable TIn>
class SinkNode : public NodeBase {
public:
    using InputType = TIn;
    
    explicit SinkNode(std::string name,
                     size_t inputCapacity = kDefaultVideoQueueCapacity)
        : NodeBase(std::move(name))
        , input(inputCapacity)
    {}
    
    /// Input pin for receiving data
    InputPin<TIn> input;
    
    void start() override {
        running_.store(true, std::memory_order_release);
    }
    
    void stop() override {
        running_.store(false, std::memory_order_release);
        input.stop();  // Wake any blocked consumers
    }
    
    void flush() override {
        input.flush();
    }
    
    /**
     * @brief Consume one input item
     * 
     * @param data Input data to consume
     */
    virtual void consume(TIn data) = 0;
};

// ============================================================================
// Typed Node Pointers
// ============================================================================

using INodePtr = std::unique_ptr<INode>;

} // namespace phoenix

