/**
 * @file command.hpp
 * @brief Command pattern base class for Undo/Redo
 * 
 * Provides the base interface for all undoable commands.
 * Commands encapsulate an operation that can be executed
 * and later undone.
 */

#pragma once

#include <string>
#include <memory>

namespace phoenix::model {

/**
 * @brief Base class for all commands
 * 
 * Commands implement the Command pattern for Undo/Redo.
 * Each command must be able to execute and undo its operation.
 * 
 * Commands should capture all state needed to undo in execute(),
 * not in the constructor.
 */
class Command {
public:
    virtual ~Command() = default;
    
    /**
     * @brief Execute the command
     * 
     * Performs the operation and stores state for undo.
     * Called when the command is first executed or redone.
     */
    virtual void execute() = 0;
    
    /**
     * @brief Undo the command
     * 
     * Reverses the operation performed by execute().
     */
    virtual void undo() = 0;
    
    /**
     * @brief Redo the command
     * 
     * Default implementation just calls execute().
     * Override if redo requires different logic.
     */
    virtual void redo() { execute(); }
    
    /**
     * @brief Get command description
     * 
     * Human-readable description for UI display.
     * E.g., "Add Clip", "Move Clip", "Delete Track"
     */
    [[nodiscard]] virtual std::string description() const = 0;
    
    /**
     * @brief Check if this command can merge with another
     * 
     * Used for combining rapid successive operations
     * (e.g., typing, dragging).
     * 
     * @param other Command to potentially merge with
     * @return true if merged (other should be discarded)
     */
    virtual bool mergeWith(const Command* other) {
        (void)other;
        return false;
    }
    
    /**
     * @brief Get command ID for merging
     * 
     * Commands with the same ID can potentially be merged.
     * Return -1 for commands that should never merge.
     */
    [[nodiscard]] virtual int id() const { return -1; }
    
    /**
     * @brief Check if command is obsolete
     * 
     * If true, the command will be removed from the undo stack.
     * Useful when a command's target no longer exists.
     */
    [[nodiscard]] virtual bool isObsolete() const { return false; }
};

/**
 * @brief Composite command (groups multiple commands)
 * 
 * Executes/undoes a group of commands as a single operation.
 */
class CompositeCommand : public Command {
public:
    explicit CompositeCommand(std::string description)
        : m_description(std::move(description)) {}
    
    void addCommand(std::unique_ptr<Command> cmd) {
        m_commands.push_back(std::move(cmd));
    }
    
    void execute() override {
        for (auto& cmd : m_commands) {
            cmd->execute();
        }
    }
    
    void undo() override {
        // Undo in reverse order
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            (*it)->undo();
        }
    }
    
    [[nodiscard]] std::string description() const override {
        return m_description;
    }
    
    [[nodiscard]] size_t commandCount() const {
        return m_commands.size();
    }
    
    [[nodiscard]] bool isEmpty() const {
        return m_commands.empty();
    }
    
private:
    std::string m_description;
    std::vector<std::unique_ptr<Command>> m_commands;
};

} // namespace phoenix::model
