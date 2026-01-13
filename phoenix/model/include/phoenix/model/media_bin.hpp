/**
 * @file media_bin.hpp
 * @brief Container for imported media items
 * 
 * MediaBin manages the collection of all imported media assets
 * in a project.
 */

#pragma once

#include <phoenix/core/uuid.hpp>
#include <phoenix/core/signals.hpp>
#include <phoenix/model/media_item.hpp>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>

namespace phoenix::model {

/**
 * @brief Container for project media assets
 * 
 * Provides O(1) lookup by UUID and maintains import order.
 */
class MediaBin {
public:
    using ItemPtr = std::shared_ptr<MediaItem>;
    
    MediaBin() = default;
    
    // ========== Item Management ==========
    
    /**
     * @brief Add a media item to the bin
     * 
     * @param item Media item to add
     * @return true if added, false if item with same ID exists
     */
    bool addItem(ItemPtr item) {
        if (!item || m_items.find(item->id()) != m_items.end()) {
            return false;
        }
        
        m_items[item->id()] = item;
        m_order.push_back(item->id());
        
        itemAdded.fire(item);
        return true;
    }
    
    /**
     * @brief Create and add a new media item
     * 
     * @param path Path to the media file
     * @return Pointer to the created item
     */
    ItemPtr addItem(const std::filesystem::path& path) {
        auto item = std::make_shared<MediaItem>(path);
        if (addItem(item)) {
            return item;
        }
        return nullptr;
    }
    
    /**
     * @brief Remove a media item by ID
     * 
     * @param id UUID of item to remove
     * @return true if removed, false if not found
     */
    bool removeItem(const UUID& id) {
        auto it = m_items.find(id);
        if (it == m_items.end()) {
            return false;
        }
        
        auto item = it->second;
        m_items.erase(it);
        
        // Remove from order
        m_order.erase(
            std::remove(m_order.begin(), m_order.end(), id),
            m_order.end()
        );
        
        itemRemoved.fire(id);
        return true;
    }
    
    /**
     * @brief Remove all items from the bin
     */
    void clear() {
        m_items.clear();
        m_order.clear();
        cleared.fire();
    }
    
    // ========== Lookup ==========
    
    /**
     * @brief Get item by UUID
     * 
     * @param id UUID to look up
     * @return Item pointer or nullptr if not found
     */
    [[nodiscard]] ItemPtr getItem(const UUID& id) const {
        auto it = m_items.find(id);
        return (it != m_items.end()) ? it->second : nullptr;
    }
    
    /**
     * @brief Find item by file path
     * 
     * @param path Path to search for
     * @return Item pointer or nullptr if not found
     */
    [[nodiscard]] ItemPtr findByPath(const std::filesystem::path& path) const {
        for (const auto& [id, item] : m_items) {
            if (item->path() == path) {
                return item;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Check if an item exists
     */
    [[nodiscard]] bool contains(const UUID& id) const {
        return m_items.find(id) != m_items.end();
    }
    
    // ========== Iteration ==========
    
    /**
     * @brief Get all items in import order
     */
    [[nodiscard]] std::vector<ItemPtr> items() const {
        std::vector<ItemPtr> result;
        result.reserve(m_order.size());
        for (const auto& id : m_order) {
            if (auto it = m_items.find(id); it != m_items.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }
    
    /**
     * @brief Get number of items
     */
    [[nodiscard]] size_t size() const {
        return m_items.size();
    }
    
    /**
     * @brief Check if bin is empty
     */
    [[nodiscard]] bool empty() const {
        return m_items.empty();
    }
    
    // ========== Signals ==========
    
    /// Emitted when an item is added
    Signal<ItemPtr> itemAdded;
    
    /// Emitted when an item is removed
    Signal<UUID> itemRemoved;
    
    /// Emitted when all items are cleared
    VoidSignal cleared;
    
private:
    std::unordered_map<UUID, ItemPtr> m_items;
    std::vector<UUID> m_order;  // Maintains import order
};

} // namespace phoenix::model
