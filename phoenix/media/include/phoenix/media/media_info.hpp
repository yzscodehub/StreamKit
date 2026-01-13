/**
 * @file media_info.hpp
 * @brief Media file information probing
 * 
 * Provides functionality to probe media files and extract
 * metadata without fully decoding the content.
 */

#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/model/media_item.hpp>
#include <string>
#include <filesystem>

namespace phoenix::media {

/**
 * @brief Media information probe
 * 
 * Probes media files to extract metadata such as duration,
 * resolution, codecs, etc.
 */
class MediaInfo {
public:
    /**
     * @brief Probe a media file and populate MediaItem
     * 
     * @param item MediaItem to populate
     * @return Success or error
     */
    static Result<void, Error> probe(model::MediaItem& item);
    
    /**
     * @brief Probe a media file and return new MediaItem
     * 
     * @param path Path to media file
     * @return Populated MediaItem or error
     */
    static Result<std::shared_ptr<model::MediaItem>, Error> probe(
        const std::filesystem::path& path);
    
    /**
     * @brief Check if a file is a supported media type
     * 
     * @param path Path to file
     * @return true if supported
     */
    static bool isSupported(const std::filesystem::path& path);
    
    /**
     * @brief Get list of supported video extensions
     */
    static const std::vector<std::string>& supportedVideoExtensions();
    
    /**
     * @brief Get list of supported audio extensions
     */
    static const std::vector<std::string>& supportedAudioExtensions();
    
    /**
     * @brief Get list of supported image extensions
     */
    static const std::vector<std::string>& supportedImageExtensions();
};

} // namespace phoenix::media
