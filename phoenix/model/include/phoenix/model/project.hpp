/**
 * @file project.hpp
 * @brief Project container holding all project data
 * 
 * A Project is the top-level container that holds:
 * - MediaBin (imported media assets)
 * - Sequences (timelines)
 * - Project settings
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/uuid.hpp>
#include <phoenix/core/signals.hpp>
#include <phoenix/model/media_bin.hpp>
#include <phoenix/model/sequence.hpp>
#include <filesystem>
#include <vector>
#include <memory>
#include <chrono>

namespace phoenix::model {

/**
 * @brief Project-wide settings
 */
struct ProjectSettings {
    std::string name{"Untitled Project"};
    
    // Default sequence settings for new sequences
    SequenceSettings defaultSequence;
    
    // Export defaults
    std::string exportPreset{"H.264 High Quality"};
    
    // Auto-save settings
    bool autoSaveEnabled = true;
    int autoSaveIntervalMinutes = 5;
    
    // Scratch disk (cache location)
    std::filesystem::path scratchDisk;
};

/**
 * @brief A video editing project
 * 
 * Contains all data for a single project:
 * - Media assets (MediaBin)
 * - Sequences (timelines)
 * - Project settings
 */
class Project {
public:
    using SequencePtr = std::shared_ptr<Sequence>;
    using MediaItemPtr = std::shared_ptr<MediaItem>;
    
    Project() : m_id(UUID::generate()) {
        // Create default sequence
        m_sequences.push_back(std::make_shared<Sequence>("Sequence 1"));
        m_activeSequenceIndex = 0;
    }
    
    explicit Project(const std::string& name)
        : m_id(UUID::generate())
    {
        m_settings.name = name;
        m_sequences.push_back(std::make_shared<Sequence>("Sequence 1"));
        m_activeSequenceIndex = 0;
    }
    
    // ========== Identification ==========
    
    [[nodiscard]] const UUID& id() const { return m_id; }
    
    [[nodiscard]] const std::string& name() const { return m_settings.name; }
    void setName(const std::string& name) { m_settings.name = name; }
    
    // ========== File Path ==========
    
    /// Project file path (.phoenix)
    [[nodiscard]] const std::filesystem::path& filePath() const { return m_filePath; }
    void setFilePath(const std::filesystem::path& path) { m_filePath = path; }
    
    /// Check if project has been saved
    [[nodiscard]] bool hasFilePath() const { return !m_filePath.empty(); }
    
    // ========== Settings ==========
    
    [[nodiscard]] const ProjectSettings& settings() const { return m_settings; }
    ProjectSettings& settings() { return m_settings; }
    void setSettings(const ProjectSettings& settings) { m_settings = settings; }
    
    // ========== Media Bin ==========
    
    [[nodiscard]] MediaBin& mediaBin() { return m_mediaBin; }
    [[nodiscard]] const MediaBin& mediaBin() const { return m_mediaBin; }
    
    /// Convenience: import media file
    MediaItemPtr importMedia(const std::filesystem::path& path) {
        return m_mediaBin.addItem(path);
    }
    
    // ========== Sequences ==========
    
    /**
     * @brief Create a new sequence
     * 
     * @param name Sequence name
     * @return Pointer to new sequence
     */
    SequencePtr createSequence(const std::string& name = "") {
        auto seq = std::make_shared<Sequence>(
            name.empty() ? "Sequence " + std::to_string(m_sequences.size() + 1) : name
        );
        seq->settings() = m_settings.defaultSequence;
        m_sequences.push_back(seq);
        
        sequenceAdded.fire(seq);
        return seq;
    }
    
    /**
     * @brief Remove a sequence
     * 
     * @param seqId UUID of sequence to remove
     * @return true if removed
     */
    bool removeSequence(const UUID& seqId) {
        auto it = std::find_if(m_sequences.begin(), m_sequences.end(),
            [&seqId](const SequencePtr& s) { return s->id() == seqId; });
        
        if (it == m_sequences.end()) return false;
        
        // Don't remove if it's the only sequence
        if (m_sequences.size() == 1) return false;
        
        auto removedIndex = static_cast<int>(std::distance(m_sequences.begin(), it));
        m_sequences.erase(it);
        
        // Adjust active sequence index if needed
        if (m_activeSequenceIndex == removedIndex) {
            // Active sequence was removed, select previous or first
            m_activeSequenceIndex = std::max(0, removedIndex - 1);
        } else if (m_activeSequenceIndex > removedIndex) {
            // Active sequence shifted down
            --m_activeSequenceIndex;
        }
        
        sequenceRemoved.fire(seqId);
        return true;
    }
    
    /**
     * @brief Get all sequences
     */
    [[nodiscard]] const std::vector<SequencePtr>& sequences() const {
        return m_sequences;
    }
    
    /**
     * @brief Get sequence by ID
     */
    [[nodiscard]] SequencePtr getSequence(const UUID& seqId) const {
        auto it = std::find_if(m_sequences.begin(), m_sequences.end(),
            [&seqId](const SequencePtr& s) { return s->id() == seqId; });
        return (it != m_sequences.end()) ? *it : nullptr;
    }
    
    /**
     * @brief Get sequence by index
     */
    [[nodiscard]] SequencePtr getSequence(size_t index) const {
        return (index < m_sequences.size()) ? m_sequences[index] : nullptr;
    }
    
    [[nodiscard]] size_t sequenceCount() const { return m_sequences.size(); }
    
    // ========== Active Sequence ==========
    
    [[nodiscard]] int activeSequenceIndex() const { return m_activeSequenceIndex; }
    
    void setActiveSequenceIndex(int index) {
        if (index >= 0 && index < static_cast<int>(m_sequences.size())) {
            m_activeSequenceIndex = index;
            activeSequenceChanged.fire(m_sequences[index]);
        }
    }
    
    [[nodiscard]] SequencePtr activeSequence() const {
        if (m_activeSequenceIndex >= 0 && 
            m_activeSequenceIndex < static_cast<int>(m_sequences.size())) {
            return m_sequences[m_activeSequenceIndex];
        }
        return nullptr;
    }
    
    // ========== Modified State ==========
    
    [[nodiscard]] bool isModified() const { return m_modified; }
    void setModified(bool modified = true) { 
        m_modified = modified;
        if (modified) {
            modifiedChanged.fire(true);
        }
    }
    void clearModified() { 
        m_modified = false;
        modifiedChanged.fire(false);
    }
    
    // ========== Timestamps ==========
    
    [[nodiscard]] std::chrono::system_clock::time_point createdTime() const {
        return m_createdTime;
    }
    void setCreatedTime(std::chrono::system_clock::time_point time) {
        m_createdTime = time;
    }
    
    [[nodiscard]] std::chrono::system_clock::time_point modifiedTime() const {
        return m_modifiedTime;
    }
    void setModifiedTime(std::chrono::system_clock::time_point time) {
        m_modifiedTime = time;
    }
    
    // ========== Signals ==========
    
    Signal<SequencePtr> sequenceAdded;
    Signal<UUID> sequenceRemoved;
    Signal<SequencePtr> activeSequenceChanged;
    Signal<bool> modifiedChanged;
    
private:
    UUID m_id;
    std::filesystem::path m_filePath;
    ProjectSettings m_settings;
    
    MediaBin m_mediaBin;
    std::vector<SequencePtr> m_sequences;
    int m_activeSequenceIndex = -1;
    
    bool m_modified = false;
    std::chrono::system_clock::time_point m_createdTime = 
        std::chrono::system_clock::now();
    std::chrono::system_clock::time_point m_modifiedTime = 
        std::chrono::system_clock::now();
};

} // namespace phoenix::model
