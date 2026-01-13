cmake_minimum_required(VERSION 3.21)
project(Phoenix VERSION 0.4.0 LANGUAGES CXX)

# ============================================================================
# C++20 Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Options
# ============================================================================
option(PHOENIX_BUILD_TESTS "Build unit tests" OFF)
option(PHOENIX_BUILD_EDITOR "Build Qt-based Editor" OFF)

# ============================================================================
# Compiler Options
# ============================================================================
if(MSVC)
    add_compile_options(
        /W4                 # Warning level 4
        /permissive-        # Strict conformance
        /Zc:__cplusplus     # Correct __cplusplus macro
        /utf-8              # UTF-8 source and execution charset
        /MP                 # Multi-processor compilation
    )
    add_compile_options(/wd4100)  # unreferenced formal parameter
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()

# ============================================================================
# Dependencies
# ============================================================================

# spdlog for logging
find_package(spdlog CONFIG REQUIRED)

# fmt
find_package(fmt CONFIG REQUIRED)

# nlohmann-json for project file serialization
find_package(nlohmann_json CONFIG REQUIRED)

# ============================================================================
# Phoenix Core Library
# ============================================================================
add_subdirectory(phoenix/core)

# ============================================================================
# Phoenix Model Library
# ============================================================================
add_subdirectory(phoenix/model)

# ============================================================================
# Phoenix Media Library (FFmpeg wrapper)
# ============================================================================
add_subdirectory(phoenix/media)

# ============================================================================
# Phoenix Engine Library (Playback/Compositor)
# ============================================================================
add_subdirectory(phoenix/engine)

# ============================================================================
# Editor Target (Qt/QML) - Phase 4 目标
# ============================================================================
if(PHOENIX_BUILD_EDITOR)
    add_subdirectory(apps/editor)
endif()

# ============================================================================
# Tests
# ============================================================================
if(PHOENIX_BUILD_TESTS)
    enable_testing()
    # TODO: 添加测试
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "Phoenix Configuration:")
message(STATUS "  Version:    ${PROJECT_VERSION}")
message(STATUS "  C++:        ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Editor:     ${PHOENIX_BUILD_EDITOR}")
message(STATUS "  Tests:      ${PHOENIX_BUILD_TESTS}")
message(STATUS "")
