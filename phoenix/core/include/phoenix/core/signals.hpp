/**
 * @file signals.hpp
 * @brief Lightweight signal-slot system (non-Qt)
 * 
 * Provides a simple, type-safe signal-slot mechanism for event handling
 * without requiring Qt. Used for decoupling components in the engine.
 * 
 * Features:
 * - Type-safe callbacks with variadic arguments
 * - Automatic disconnection via Connection RAII
 * - Thread-safe signal emission
 * - No external dependencies
 */

#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <atomic>

namespace phoenix {

// Forward declarations
template<typename... Args>
class Signal;

/**
 * @brief Connection handle for managing slot lifetime
 * 
 * RAII wrapper that automatically disconnects when destroyed.
 * Can also be manually disconnected.
 */
class Connection {
public:
    Connection() = default;
    
    /// Check if connection is active
    [[nodiscard]] bool connected() const {
        return m_connected && !m_disconnector.expired();
    }
    
    /// Disconnect the slot
    void disconnect() {
        if (auto disc = m_disconnector.lock()) {
            disc->disconnect(m_id);
        }
        m_connected = false;
    }
    
    /// Block the connection temporarily
    void block() { m_blocked = true; }
    
    /// Unblock the connection
    void unblock() { m_blocked = false; }
    
    /// Check if blocked
    [[nodiscard]] bool blocked() const { return m_blocked; }
    
private:
    template<typename... Args>
    friend class Signal;
    
    /// Internal disconnector interface
    struct Disconnector {
        virtual ~Disconnector() = default;
        virtual void disconnect(uint64_t id) = 0;
    };
    
    Connection(uint64_t id, std::shared_ptr<Disconnector> disc)
        : m_id(id), m_disconnector(disc), m_connected(true) {}
    
    uint64_t m_id = 0;
    std::weak_ptr<Disconnector> m_disconnector;
    bool m_connected = false;
    bool m_blocked = false;
};

/**
 * @brief Scoped connection that auto-disconnects on destruction
 */
class ScopedConnection {
public:
    ScopedConnection() = default;
    
    explicit ScopedConnection(Connection conn)
        : m_connection(std::move(conn)) {}
    
    ~ScopedConnection() {
        disconnect();
    }
    
    // Move only
    ScopedConnection(ScopedConnection&& other) noexcept
        : m_connection(std::move(other.m_connection)) {}
    
    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            disconnect();
            m_connection = std::move(other.m_connection);
        }
        return *this;
    }
    
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    
    /// Disconnect manually
    void disconnect() {
        m_connection.disconnect();
    }
    
    /// Check if connected
    [[nodiscard]] bool connected() const {
        return m_connection.connected();
    }
    
    /// Get underlying connection
    Connection& connection() { return m_connection; }
    const Connection& connection() const { return m_connection; }
    
private:
    Connection m_connection;
};

/**
 * @brief Type-safe signal for event emission
 * 
 * @tparam Args Argument types passed to connected slots
 * 
 * Usage:
 * @code
 *   Signal<int, std::string> mySignal;
 *   
 *   auto conn = mySignal.connect([](int a, const std::string& b) {
 *       std::cout << a << ": " << b << std::endl;
 *   });
 *   
 *   mySignal.emit(42, "hello");  // Prints: 42: hello
 *   
 *   conn.disconnect();  // Or let ScopedConnection handle it
 * @endcode
 */
template<typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;
    
    Signal() : m_disconnector(std::make_shared<DisconnectorImpl>(this)) {}
    
    ~Signal() {
        // Prevent dangling disconnector references
        m_disconnector->m_signal = nullptr;
    }
    
    // Non-copyable, non-movable (slots hold references)
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = delete;
    Signal& operator=(Signal&&) = delete;
    
    /**
     * @brief Connect a slot to this signal
     * 
     * @param slot Callable to invoke when signal is emitted
     * @return Connection handle for managing the slot
     */
    [[nodiscard]] Connection connect(SlotType slot) {
        std::lock_guard lock(m_mutex);
        
        uint64_t id = m_nextId++;
        m_slots.push_back({id, std::move(slot), false});
        
        return Connection(id, m_disconnector);
    }
    
    /**
     * @brief Connect and return scoped connection
     */
    [[nodiscard]] ScopedConnection connectScoped(SlotType slot) {
        return ScopedConnection(connect(std::move(slot)));
    }
    
    /**
     * @brief Fire the signal, invoking all connected slots
     * 
     * Thread-safe: slots are copied before invocation to prevent
     * issues if a slot modifies connections during emission.
     * 
     * Note: Named 'fire' instead of 'emit' to avoid conflict with Qt's emit keyword.
     */
    void fire(Args... args) {
        // Copy slots under lock
        std::vector<Slot> slotsCopy;
        {
            std::lock_guard lock(m_mutex);
            slotsCopy = m_slots;
        }
        
        // Invoke outside lock
        for (auto& slot : slotsCopy) {
            if (!slot.blocked && slot.func) {
                slot.func(args...);
            }
        }
    }
    
    /**
     * @brief Fire using operator()
     */
    void operator()(Args... args) {
        fire(std::forward<Args>(args)...);
    }
    
    /**
     * @brief Disconnect all slots
     */
    void disconnectAll() {
        std::lock_guard lock(m_mutex);
        m_slots.clear();
    }
    
    /**
     * @brief Get number of connected slots
     */
    [[nodiscard]] size_t slotCount() const {
        std::lock_guard lock(m_mutex);
        return m_slots.size();
    }
    
    /**
     * @brief Check if any slots are connected
     */
    [[nodiscard]] bool hasConnections() const {
        return slotCount() > 0;
    }
    
private:
    struct Slot {
        uint64_t id;
        SlotType func;
        bool blocked;
    };
    
    /// Disconnector implementation
    struct DisconnectorImpl : Connection::Disconnector {
        Signal* m_signal;
        
        explicit DisconnectorImpl(Signal* sig) : m_signal(sig) {}
        
        void disconnect(uint64_t id) override {
            if (m_signal) {
                m_signal->disconnectById(id);
            }
        }
    };
    
    void disconnectById(uint64_t id) {
        std::lock_guard lock(m_mutex);
        m_slots.erase(
            std::remove_if(m_slots.begin(), m_slots.end(),
                [id](const Slot& s) { return s.id == id; }),
            m_slots.end()
        );
    }
    
    mutable std::mutex m_mutex;
    std::vector<Slot> m_slots;
    std::shared_ptr<DisconnectorImpl> m_disconnector;
    std::atomic<uint64_t> m_nextId{1};
};

/**
 * @brief Convenience alias for void signals (no arguments)
 */
using VoidSignal = Signal<>;

} // namespace phoenix
