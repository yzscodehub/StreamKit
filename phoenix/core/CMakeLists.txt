# phoenix/core - Core library (basic types, utilities, logging)

add_library(phoenix_core
    src/logger.cpp
    src/uuid.cpp
)

target_include_directories(phoenix_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(phoenix_core
    PUBLIC
        spdlog::spdlog
        fmt::fmt
)

target_compile_features(phoenix_core PUBLIC cxx_std_20)

# Alias for convenient linking
add_library(phoenix::core ALIAS phoenix_core)
