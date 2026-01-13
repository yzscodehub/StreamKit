/**
 * @file project_io.hpp
 * @brief Project serialization to/from JSON
 * 
 * Provides save/load functionality for Phoenix projects.
 * Project files use the .phoenix extension (JSON format).
 */

#pragma once

#include <phoenix/core/result.hpp>
#include <phoenix/model/project.hpp>
#include <filesystem>
#include <string>

namespace phoenix::model {

/**
 * @brief Project file I/O operations
 */
class ProjectIO {
public:
    /// File extension for project files
    static constexpr const char* kFileExtension = ".phoenix";
    
    /// Current file format version
    static constexpr int kFormatVersion = 1;
    
    /**
     * @brief Save project to file
     * 
     * @param project Project to save
     * @param path Output file path
     * @return Result indicating success or error
     */
    static Result<void, Error> save(
        const Project& project,
        const std::filesystem::path& path
    );
    
    /**
     * @brief Load project from file
     * 
     * @param path Path to project file
     * @return Loaded project or error
     */
    static Result<std::unique_ptr<Project>, Error> load(
        const std::filesystem::path& path
    );
    
    /**
     * @brief Export project to JSON string
     * 
     * @param project Project to export
     * @return JSON string
     */
    static std::string toJson(const Project& project);
    
    /**
     * @brief Import project from JSON string
     * 
     * @param json JSON string
     * @return Loaded project or error
     */
    static Result<std::unique_ptr<Project>, Error> fromJson(
        const std::string& json
    );
};

} // namespace phoenix::model
