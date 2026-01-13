/**
 * @file undo_stack.hpp
 * @brief Undo/Redo stack for managing commands
 * 
 * Provides a stack-based undo/redo system that manages
 * command history.
 */

#pragma once

#include <phoenix/core/signals.hpp>
#include <phoenix/model/commands/command.hpp>
#include <vector>
#include <memory>
#include <string>

namespace phoenix::model {

/**
 * @brief Undo/Redo stack
 * 
 * Manages a history of executed commands that can be undone
 * and redone. Commands are executed through the stack to
 * ensure proper history tracking.
 * 
 * Usage:
 * @code
 *   UndoStack stack;
 *   stack.push(std::make_unique<AddClipCommand>(...));
 *   stack.undo();  // Undoes AddClip
 *   stack.redo();  // Redoes AddClip
 * @endcode
 */
class UndoStack {
public:
    UndoStack() = default;
    
    // Non-copyable
    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;
    
    // ========== Command Execution ==========
    
    /**
     * @brief Execute a command and push to undo stack
     * 
     * @param command Command to execute
     */
    void push(std::unique_ptr<Command> command) {
        if (!command) return;
        
        // Clear redo stack
        m_redoStack.clear();
        
        // Execute the command
        command->execute();
        
        // Try to merge with previous command
        if (!m_undoStack.empty() && 
            m_undoStack.back()->id() != -1 &&
            m_undoStack.back()->id() == command->id()) {
            if (m_undoStack.back()->mergeWith(command.get())) {
                // Merged - don't add new command
                indexChanged.fire();
                return;
            }
        }
        
        // Add to undo stack
        m_undoStack.push_back(std::move(command));
        
        // Limit stack size
        while (m_undoStack.size() > m_undoLimit) {
            m_undoStack.erase(m_undoStack.begin());
        }
        
        m_cleanIndex = -1;  // Mark as modified
        indexChanged.fire();
    }
    
    // ========== Undo/Redo ==========
    
    /**
     * @brief Undo the last command
     */
    void undo() {
        if (!canUndo()) return;
        
        auto& cmd = m_undoStack.back();
        cmd->undo();
        
        m_redoStack.push_back(std::move(cmd));
        m_undoStack.pop_back();
        
        indexChanged.fire();
    }
    
    /**
     * @brief Redo the last undone command
     */
    void redo() {
        if (!canRedo()) return;
        
        auto& cmd = m_redoStack.back();
        cmd->redo();
        
        m_undoStack.push_back(std::move(cmd));
        m_redoStack.pop_back();
        
        indexChanged.fire();
    }
    
    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool canUndo() const {
        return !m_undoStack.empty();
    }
    
    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool canRedo() const {
        return !m_redoStack.empty();
    }
    
    /**
     * @brief Get undo command description
     */
    [[nodiscard]] std::string undoText() const {
        if (!canUndo()) return "";
        return m_undoStack.back()->description();
    }
    
    /**
     * @brief Get redo command description
     */
    [[nodiscard]] std::string redoText() const {
        if (!canRedo()) return "";
        return m_redoStack.back()->description();
    }
    
    // ========== Stack Management ==========
    
    /**
     * @brief Clear all history
     */
    void clear() {
        m_undoStack.clear();
        m_redoStack.clear();
        m_cleanIndex = 0;
        indexChanged.fire();
    }
    
    /**
     * @brief Get current index (number of commands executed)
     */
    [[nodiscard]] int index() const {
        return static_cast<int>(m_undoStack.size());
    }
    
    /**
     * @brief Get total command count (undo + redo)
     */
    [[nodiscard]] size_t count() const {
        return m_undoStack.size() + m_redoStack.size();
    }
    
    /**
     * @brief Get undo stack size
     */
    [[nodiscard]] size_t undoCount() const {
        return m_undoStack.size();
    }
    
    /**
     * @brief Get redo stack size
     */
    [[nodiscard]] size_t redoCount() const {
        return m_redoStack.size();
    }
    
    // ========== Clean State ==========
    
    /**
     * @brief Mark current state as clean (saved)
     */
    void setClean() {
        m_cleanIndex = static_cast<int>(m_undoStack.size());
        cleanChanged.fire(true);
    }
    
    /**
     * @brief Check if current state is clean
     */
    [[nodiscard]] bool isClean() const {
        return m_cleanIndex == static_cast<int>(m_undoStack.size());
    }
    
    // ========== Configuration ==========
    
    /**
     * @brief Set maximum undo history size
     */
    void setUndoLimit(size_t limit) {
        m_undoLimit = limit;
        while (m_undoStack.size() > m_undoLimit) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }
    
    [[nodiscard]] size_t undoLimit() const {
        return m_undoLimit;
    }
    
    // ========== Signals ==========
    
    /// Emitted when index changes (after push, undo, redo)
    VoidSignal indexChanged;
    
    /// Emitted when clean state changes
    Signal<bool> cleanChanged;
    
private:
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    
    int m_cleanIndex = 0;
    size_t m_undoLimit = 100;
};

} // namespace phoenix::model
