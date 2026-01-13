# phoenix/engine - Playback and rendering engine
cmake_minimum_required(VERSION 3.20)

project(phoenix_engine CXX)

# ============================================================================
# Source files
# ============================================================================

set(ENGINE_SOURCES
    # Currently header-only, but prepared for implementation files
    # src/playback_engine.cpp
    # src/compositor.cpp
    # src/frame_cache.cpp
)

set(ENGINE_HEADERS
    include/phoenix/engine/frame_cache.hpp
    include/phoenix/engine/compositor.hpp
    include/phoenix/engine/playback_engine.hpp
)

# ============================================================================
# Library target
# ============================================================================

# Header-only library for now
add_library(phoenix_engine INTERFACE)

# Include directories
target_include_directories(phoenix_engine INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ============================================================================
# Dependencies
# ============================================================================

# Internal dependencies
target_link_libraries(phoenix_engine INTERFACE
    phoenix_core
    phoenix_model
    phoenix_media
)

# ============================================================================
# Compiler features
# ============================================================================

target_compile_features(phoenix_engine INTERFACE cxx_std_20)

# ============================================================================
# IDE organization
# ============================================================================

source_group("Header Files" FILES ${ENGINE_HEADERS})
